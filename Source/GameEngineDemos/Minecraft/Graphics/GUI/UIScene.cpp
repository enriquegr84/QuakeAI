/*
Minetest
Copyright (C) 2020 Jean-Patrick Guerrero <jeanpatrick.guerrero@gmail.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as published by
the Free Software Foundation; either version 2.1 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#include "UIScene.h"

#include "Core/OS/OS.h"

#include "Graphic/Renderer/Renderer.h"

UIScene::UIScene(Scene* pScene, BaseUI* ui, int id, RectangleShape<2, int> rectangle) :
    BaseUIElement(UIET_ELEMENT, id, rectangle), mScene(pScene), mUI(ui)
{
    float upFov, aspectRatio, dMin, dMax;

    mCamera = mScene->AddCameraNode();
    mCamera->GetRelativeTransform().SetTranslation(Vector3<float>{0.f, 0.f, -100.f});
    mCamera->Get()->GetFrustum(upFov, aspectRatio, dMin, dMax);
    mCamera->Get()->SetFrustum(30.f, aspectRatio, dMin, dMax);

	//mScene->GetParameters()->SetAttribute(scene::ALLOW_ZWRITE_ON_TRANSPARENT, true);

    //basic visual effect
    {
        // Create a vertex buffer for a single triangle.
        VertexFormat vformat;
        vformat.Bind(VA_POSITION, DF_R32G32B32_FLOAT, 0);
        vformat.Bind(VA_COLOR, DF_R32G32B32A32_FLOAT, 0);

        std::shared_ptr<VertexBuffer> vbuffer = std::make_shared<VertexBuffer>(vformat, 4);
        std::shared_ptr<IndexBuffer> ibuffer = std::make_shared<IndexBuffer>(IP_TRISTRIP, 2);
        vbuffer->SetUsage(Resource::DYNAMIC_UPDATE);

        std::vector<std::string> path;
#if defined(_OPENGL_)
        path.push_back("Effects/ColorEffectVS.glsl");
        path.push_back("Effects/ColorEffectPS.glsl");
#else
        path.push_back("Effects/ColorEffectVS.hlsl");
        path.push_back("Effects/ColorEffectPS.hlsl");
#endif
		std::shared_ptr<ResHandle> resHandle =
			ResCache::Get()->GetHandle(&BaseResource(ToWideString(path.front())));

		const std::shared_ptr<ShaderResourceExtraData>& extra =
			std::static_pointer_cast<ShaderResourceExtraData>(resHandle->GetExtra());
		if (!extra->GetProgram())
			extra->GetProgram() = ProgramFactory::Get()->CreateFromFiles(path.front(), path.back(), "");

        std::shared_ptr<VisualEffect> effect = std::make_shared<ColorEffect>(
			ProgramFactory::Get()->CreateFromProgram(extra->GetProgram()));

        // Create the geometric object for drawing.
        mVisual = std::make_shared<Visual>(vbuffer, ibuffer, effect);
    }
}

UIScene::~UIScene()
{
	SetMesh(nullptr);
}

std::shared_ptr<AnimatedMeshNode> UIScene::SetMesh(std::shared_ptr<AnimatedMesh> mesh)
{
	if (mMeshNode) 
    {
        mScene->GetRootNode()->DetachChild(mMeshNode);
		mMeshNode = nullptr;
	}

	if (!mesh)
		return nullptr;

	mMeshNode = mScene->AddAnimatedMeshNode(0, mesh);
	mMeshNode->GetRelativeTransform().SetTranslation(-mMeshNode->GetVisual(0)->mModelBound.GetCenter());
	mMeshNode->AnimateJoints();

	return mMeshNode;
}

void UIScene::SetTexture(unsigned int idx, std::shared_ptr<Texture2> texture)
{
	std::shared_ptr<Material> material = mMeshNode->GetMaterial(idx);
	material->mType = MT_TRANSPARENT_ALPHA_CHANNEL;
	material->mTypeParam = 0.5f;
	material->mTextureLayer[0].mTexture = texture;
	material->mTextureLayer[0].mFilter = SamplerState::Filter::ANISOTROPIC;
    material->mLighting = false;
	//material->SetFlag(EMF_FOG_ENABLE, true);
	//material->SetFlag(EMF_BILINEAR_FILTER, false);

	material->mBlendTarget.enable = true;
	material->mBlendTarget.srcColor = BlendState::BM_SRC_ALPHA;
	material->mBlendTarget.dstColor = BlendState::BM_INV_SRC_ALPHA;
	material->mBlendTarget.srcAlpha = BlendState::BM_SRC_ALPHA;
	material->mBlendTarget.dstAlpha = BlendState::BM_INV_SRC_ALPHA;

	material->mDepthBuffer = true;
	material->mDepthMask = DepthStencilState::MASK_ALL;

	material->mFillMode = RasterizerState::FILL_SOLID;
	material->mCullMode = RasterizerState::CULL_NONE;
}

void UIScene::Draw()
{
    std::shared_ptr<BaseUISkin> skin = mUI->GetSkin();
    if (!skin)
        return;

	Renderer::Get()->ClearDepthBuffer();

	// Control rotation speed based on time
    unsigned int newTime = Timer::GetTime();
    unsigned int dTime = 0;
	if (mLastTime != 0)
        dTime = std::abs((int)mLastTime - (int)newTime);
	mLastTime = newTime;

    int viewX, viewY, viewW, viewH;
    Renderer::Get()->GetViewport(viewX, viewY, viewW, viewH);

    RectangleShape<2, int> rect = GetAbsoluteClippingRect();
    Vector2<int> upperLeft = rect.GetVertice(RVP_UPPERLEFT);
    Renderer::Get()->SetViewport(
        upperLeft[0], upperLeft[1], rect.mExtent[0], rect.mExtent[1]);
    
	RectangleShape<2, int> borderRect = mUI->GetRootUIElement()->GetAbsoluteClippingRect();
	if (mBGColor != 0) 
        skin->Draw3DSunkenPane(mBGColor, false, true, mVisual, borderRect, 0);

    float upFov, aspectRatio, dMin, dMax;
    mScene->GetActiveCamera()->Get()->GetFrustum(upFov, aspectRatio, dMin, dMax);
    mScene->GetActiveCamera()->Get()->SetFrustum(
        upFov, (float)rect.mExtent[0] / (float)rect.mExtent[1], dMin, dMax);

	if (!mCamera->GetTarget()) 
    {
		UpdateCamera(mScene->AddEmptyNode(0));
		RotateCamera(EulerAngles<float>());
		//mCamera->BindTargetAndRotation(true);
	}

	CameraLoop();

	// Continuous rotation
	if (mInfRot)
	{
		float yaw = 0.f;
		float pitch = -0.03f * (float)dTime;
		float roll = 0.f;

		EulerAngles<float> yawPitchRoll;
		yawPitchRoll.mAxis[1] = 1;
		yawPitchRoll.mAxis[2] = 2;
		yawPitchRoll.mAngle[0] = roll;
		yawPitchRoll.mAngle[1] = yaw;
		yawPitchRoll.mAngle[2] = pitch;
		RotateCamera(yawPitchRoll);
	}

	mScene->OnRender();

	if (mInitialRotation && mMeshNode)
    {
		float yaw = 0.f;
		float pitch = mCustomRot[1];
		float roll = mCustomRot[0];

		EulerAngles<float> yawPitchRoll;
		yawPitchRoll.mAxis[1] = 1;
		yawPitchRoll.mAxis[2] = 2;
		yawPitchRoll.mAngle[0] = roll;
		yawPitchRoll.mAngle[1] = yaw;
		yawPitchRoll.mAngle[2] = pitch;
		RotateCamera(yawPitchRoll);
		CalculateOptimalDistance();

		mInitialRotation = false;
	}

	Renderer::Get()->SetViewport(viewX, viewY, viewW, viewH);
}

bool UIScene::OnEvent(const Event& evt)
{
	if (mMouseCtrl && evt.mEventType == ET_MOUSE_INPUT_EVENT)
    {
		if (evt.mMouseInput.mEvent == MIE_LMOUSE_PRESSED_DOWN)
        {
            mCursorLastPos = Vector2<float>{ (float)evt.mMouseInput.X, (float)evt.mMouseInput.Y };
			return true;
		} 
        else if (evt.mMouseInput.mEvent == MIE_MOUSE_MOVED)
        {
			if (evt.mMouseInput.IsLeftPressed())
            {
                mCursorPos = Vector2<float>{ (float)evt.mMouseInput.X, (float)evt.mMouseInput.Y };

				float yaw = 0.f;
				float pitch = mCursorPos[0] - mCursorLastPos[0];
				float roll = mCursorLastPos[1] - mCursorPos[1];

				EulerAngles<float> yawPitchRoll;
				yawPitchRoll.mAxis[1] = 1;
				yawPitchRoll.mAxis[2] = 2;
				yawPitchRoll.mAngle[0] = roll;
				yawPitchRoll.mAngle[1] = yaw;
				yawPitchRoll.mAngle[2] = pitch;
                RotateCamera(yawPitchRoll);

				mCursorLastPos = mCursorPos;
				return true;
			}
		}
	}

	return BaseUIElement::OnEvent(evt);
}

void UIScene::SetStyles(const std::array<Style, Style::NUM_STATES> &styles)
{
	Style::State state = Style::STATE_DEFAULT;
	Style style = Style::GetStyleFromStatePropagation(styles, state);

	SetNotClipped(style.GetBool(Style::NOCLIP, false));
	SetBackgroundColor(style.GetColor(Style::BGCOLOR, mBGColor));
}

/**
 * Sets the frame loop range for the mesh
 */
void UIScene::SetFrameLoop(int begin, int end)
{
	if (mMeshNode->GetStartFrame() != begin || mMeshNode->GetEndFrame() != end)
		mMeshNode->SetFrameLoop(begin, end);
}

/**
 * Sets the animation speed (FPS) for the mesh
 */
void UIScene::SetAnimationSpeed(float speed)
{
	mMeshNode->SetAnimationSpeed(speed);
}

/* Camera control functions */
inline void UIScene::CalculateOptimalDistance()
{
    BoundingBox<float> box;// = mMesh->GetBoundingBox();
	float width  = box.mMaxEdge[0] - box.mMinEdge[0];
	float height = box.mMaxEdge[1] - box.mMinEdge[1];
	float depth  = box.mMaxEdge[2] - box.mMinEdge[2];
	float maxWidth = width > depth ? width : depth;

    const float* frustum = mCamera->Get()->GetFrustum();
	float camFar = mCamera->Get()->GetDMax();
	float farWidth = camFar;
	float farHeight = camFar;

    RectangleShape<2, int> rect = GetAbsolutePosition();
	float zoomX = rect.mExtent[0] / maxWidth;
	float zoomY = rect.mExtent[1] / height;
	float dist;

	if (zoomX < zoomY)
		dist = (maxWidth / (farWidth / camFar)) + (0.5f * maxWidth);
	else
		dist = (height / (farHeight / camFar)) + (0.5f * maxWidth);

	mCameraDistance = dist;
	mUpdateCamera = true;
}

void UIScene::UpdateCamera(std::shared_ptr<Node> target)
{
    mCamera->SetTarget(target);
	UpdateTargetPosition();

	mLastTargetPos = mTargetPos;
	UpdateCameraPosition();

	mUpdateCamera = true;
}

void UIScene::UpdateTargetPosition()
{
	mLastTargetPos = mTargetPos;
	mCamera->GetTarget()->UpdateAbsoluteTransform();
	mTargetPos = mCamera->GetTarget()->GetAbsoluteTransform().GetTranslation();
}

void UIScene::SetCameraRotation(EulerAngles<float> rot)
{
	CorrectBounds(rot);

    Transform transform;
    transform.SetRotation(rot);

	Vector4<float> cameraPos = Vector4<float>{ 0.f, 0.f, mCameraDistance, 0.f };
    transform.GetMatrix().Transformation(cameraPos);
    mCameraPos = HProject(cameraPos);

	mCameraPos += mTargetPos;
	mCamera->GetRelativeTransform().SetTranslation(mCameraPos);
	mUpdateCamera = false;
}

bool UIScene::CorrectBounds(EulerAngles<float>& rot)
{
	const float ROTATION_MAX_1 = 60.0f * GE_C_DEG_TO_RAD;
	const float ROTATION_MAX_2 = 300.0f * GE_C_DEG_TO_RAD;

	// Limit and correct the rotation when needed
	if (rot.mAngle[0] < 90.f * GE_C_DEG_TO_RAD)
    {
		if (rot.mAngle[0] > ROTATION_MAX_1)
        {
			rot.mAngle[0] = ROTATION_MAX_1;
			return true;
		}
	} 
    else if (rot.mAngle[0] < ROTATION_MAX_2) 
    {
		rot.mAngle[0] = ROTATION_MAX_2;
		return true;
	}

	// Not modified
	return false;
}

void UIScene::CameraLoop()
{
	UpdateCameraPosition();
	UpdateTargetPosition();

	if (mTargetPos != mLastTargetPos)
		mUpdateCamera = true;

	if (mUpdateCamera) 
    {
        mCameraPos -= mTargetPos;
        Normalize(mCameraPos);
		mCameraPos = mTargetPos + mCameraPos * mCameraDistance;

		EulerAngles<float> rot = GetCameraRotation();
		if (CorrectBounds(rot))
			SetCameraRotation(rot);

		mCamera->GetRelativeTransform().SetTranslation(mCameraPos);

		mUpdateCamera = false;
	}
}

void UIScene::UpdateCameraPosition() 
{ 
    mCameraPos = mCamera->GetRelativeTransform().GetTranslation();
};

EulerAngles<float> UIScene::GetCameraRotation() const
{
    EulerAngles<float> rotation;
    rotation.mAxis[1] = 1;
    rotation.mAxis[2] = 2;

    Quaternion<float> q(HLift(mCameraPos, 0.f) - HLift(mTargetPos, 0.f));
    rotation = Rotation<4, float>(q)(rotation.mAxis[0], rotation.mAxis[1], rotation.mAxis[2]);
    return rotation;
};

void UIScene::RotateCamera(const EulerAngles<float>& rotation)
{
    Quaternion<float> q1 = Rotation<4, float>(rotation);
    Quaternion<float> q2 = Rotation<4, float>(GetCameraRotation());

    EulerAngles<float> cameraRotation;
    cameraRotation.mAxis[1] = 1;
    cameraRotation.mAxis[2] = 2;
    SetCameraRotation(Rotation<4, float>(q1 + q2)
        (cameraRotation.mAxis[0], cameraRotation.mAxis[1], cameraRotation.mAxis[2]));
};

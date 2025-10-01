#include "UIVolumeChange.h"

#include "Graphic/UI/UIEngine.h"

#include "Core/Utility/Serialize.h"

#include "Application/Settings.h"

const int IDSoundText = 263;
const int IDSoundExitButton = 264;
const int IDSoundSlider = 265;
const int IDSoundMuteButton = 266;

UIVolumeChange::UIVolumeChange(BaseUI* ui, int id, RectangleShape<2, int> rectangle) :
    BaseUIForm(ui, id, rectangle)
{
	mBlendState = std::make_shared<BlendState>();
	mBlendState->mTarget[0].enable = true;
	mBlendState->mTarget[0].srcColor = BlendState::BM_SRC_ALPHA;
	mBlendState->mTarget[0].dstColor = BlendState::BM_INV_SRC_ALPHA;
	mBlendState->mTarget[0].srcAlpha = BlendState::BM_SRC_ALPHA;
	mBlendState->mTarget[0].dstAlpha = BlendState::BM_INV_SRC_ALPHA;

    // Create a vertex buffer for a single triangle.
    VertexFormat vformat;
    vformat.Bind(VA_POSITION, DF_R32G32B32_FLOAT, 0);
    vformat.Bind(VA_COLOR, DF_R32G32B32A32_FLOAT, 0);

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

    mEffect = std::make_shared<ColorEffect>(
		ProgramFactory::Get()->CreateFromProgram(extra->GetProgram()));

    std::shared_ptr<IndexBuffer> ibuffer = std::make_shared<IndexBuffer>(IP_TRISTRIP, 2);
    std::shared_ptr<VertexBuffer> vbuffer = std::make_shared<VertexBuffer>(vformat, 4);
    vbuffer->SetUsage(Resource::DYNAMIC_UPDATE);

    // Create the geometric object for drawing.
    mVisual = std::make_shared<Visual>(vbuffer, ibuffer, mEffect);
}

UIVolumeChange::~UIVolumeChange()
{
	RemoveChildren(true);
}

void UIVolumeChange::RemoveChildren(bool cascade)
{
	if (std::shared_ptr<BaseUIElement> el = GetElementFromId(IDSoundText))
        el->Remove();

	if (std::shared_ptr<BaseUIElement> el = GetElementFromId(IDSoundExitButton))
		el->Remove();

	if (std::shared_ptr<BaseUIElement> el = GetElementFromId(IDSoundSlider))
		el->Remove();

	if (std::shared_ptr<BaseUIElement> el = GetElementFromId(IDSoundMuteButton))
		el->Remove();
}

void UIVolumeChange::RegenerateUI(Vector2<unsigned int> screenSize)
{
	/*
		Remove stuff
	*/
	RemoveChildren(true);
	/*
		Calculate new sizes and positions
	*/
    const float s = Settings::Get()->GetFloat("gui_scaling");
    mDesiredRect = RectangleShape<2, int>();
    mDesiredRect.mExtent = Vector2<int>{ (int)(380 * s), (int)(200 * s) };
    mDesiredRect.mCenter = Vector2<int>{ (int)(screenSize[0] / 2), (int)(screenSize[1] / 2) };
    RecalculateAbsolutePosition(false);

	int volume = (int)(Settings::Get()->GetFloat("sound_volume") * 100);

	/*
		Add stuff
	*/
	{
        RectangleShape<2, int> rect;
        rect.mExtent = Vector2<int>{ (int)(160 * s), (int)(20 * s) };
        rect.mCenter = rect.mExtent / 2;
        rect.mCenter += Vector2<int>{
            (int)(mDesiredRect.mExtent[0] / 2 - 80 * s), 
            (int)(mDesiredRect.mExtent[1] / 2 - 70 * s)};

		std::wstring volumeText = L"Sound Volume: ";
        volumeText += std::to_wstring(volume) + L"%";
        mUI->AddStaticText(volumeText.c_str(), rect, false,
				true, shared_from_this(), IDSoundText);
	}
	{
        RectangleShape<2, int> rect;
        rect.mExtent = Vector2<int>{ (int)(80 * s), (int)(30 * s) };
        rect.mCenter = rect.mExtent / 2;
        rect.mCenter += Vector2<int>{
            (int)(mDesiredRect.mExtent[0] / 2 - 80 * s / 2),
            (int)(mDesiredRect.mExtent[1] / 2 + 55 * s)};

        std::wstring text = L"Exit";
        mUI->AddButton(rect, shared_from_this(), IDSoundExitButton, text.c_str());
	}
	{
        RectangleShape<2, int> rect;
        rect.mExtent = Vector2<int>{ (int)(300 * s), (int)(20 * s) };
        rect.mCenter = rect.mExtent / 2;
        rect.mCenter += Vector2<int>{
            (int)(mDesiredRect.mExtent[0] / 2 - 150 * s),
            (int)(mDesiredRect.mExtent[1] / 2)};

        auto el = mUI->AddScrollBar(true, true, rect, shared_from_this(), IDSoundSlider);
		el->SetMax(100);
		el->SetPageSize((int)(rect.mExtent[0] - 2 * rect.mExtent[1]) * 10);
		el->SetPosition(volume);
	}
	{
        RectangleShape<2, int> rect;
        rect.mExtent = Vector2<int>{ (int)(160 * s), (int)(20 * s) };
        rect.mCenter = rect.mExtent / 2;
        rect.mCenter += Vector2<int>{
            (int)(mDesiredRect.mExtent[0] / 2 - 80 * s),
            (int)(mDesiredRect.mExtent[1] / 2 - 35 * s)};

        std::wstring text = L"Muted";
        mUI->AddCheckBox(Settings::Get()->GetBool("mute_sound"), 
            rect, shared_from_this(), IDSoundMuteButton, text.c_str());
	}
}

void UIVolumeChange::DrawForm()
{
	std::shared_ptr<BaseUISkin> skin = mUI->GetSkin();
	if (!skin)
		return;

	Renderer::Get()->SetBlendState(mBlendState);

	SColor bgColor(140, 0, 0, 0);
    skin->Draw2DRectangle(bgColor, mVisual, mAbsoluteRect, &mAbsoluteClippingRect);

	Renderer::Get()->SetDefaultBlendState();

	BaseUIElement::Draw();
}

bool UIVolumeChange::OnEvent(const Event& evt)
{
	if (evt.mEventType == ET_KEY_INPUT_EVENT)
    {
		if (evt.mKeyInput.mKey == KEY_ESCAPE && evt.mKeyInput.mPressedDown)
        {
			QuitForm();
			return true;
		}

		if (evt.mKeyInput.mKey == KEY_RETURN && evt.mKeyInput.mPressedDown)
        {
			QuitForm();
			return true;
		}
	} 
    else if (evt.mEventType == ET_UI_EVENT)
    {
		if (evt.mUIEvent.mEventType == UIEVT_CHECKBOX_CHANGED)
        {
			std::shared_ptr<BaseUIElement> el = GetElementFromId(IDSoundMuteButton);
			if (el != NULL && el->GetType() == UIET_CHECK_BOX)
				Settings::Get()->SetBool("mute_sound", 
                    std::static_pointer_cast<BaseUICheckBox>(el)->IsChecked());

			mUI->SetFocus(shared_from_this());
			return true;
		}

		if (evt.mUIEvent.mEventType == UIEVT_BUTTON_CLICKED)
        {
			if (evt.mUIEvent.mCaller->GetID() == IDSoundExitButton)
            {
				QuitForm();
				return true;
			}
            mUI->SetFocus(shared_from_this());
		}

		if (evt.mUIEvent.mEventType == UIEVT_ELEMENT_FOCUS_LOST && IsVisible())
        {
			if (!CanTakeFocus(evt.mUIEvent.mElement))
            {
				LogInformation("UIVolumeChange: Not allowing focus change.");
				// Returning true disables focus change
				return true;
			}
		}

		if (evt.mUIEvent.mEventType == UIEVT_SCROLL_BAR_CHANGED)
        {
			if (evt.mUIEvent.mCaller->GetID() == IDSoundSlider)
            {
				int pos = ((BaseUIScrollBar*)evt.mUIEvent.mCaller)->GetPosition();
				Settings::Get()->SetFloat("sound_volume", (float) pos / 100);

				std::shared_ptr<BaseUIElement> el = GetElementFromId(IDSoundText);

                std::wstring volumeText = L"Sound Volume: ";
                volumeText += std::to_wstring(pos) + L"%";
				el->SetText(volumeText.c_str());
				return true;
			}
		}
	}

	return mParent ? mParent->OnEvent(evt) : false;
}

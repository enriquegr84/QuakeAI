/*
Minetest
Copyright (C) 2010-2013 celeron55, Perttu Ahola <celeron55@gmail.com>

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

#ifndef NODE_H
#define NODE_H

#include "../MinecraftStd.h"

#include "Tile.h"
#include "TextureOverride.h"

#include "../Games/Map/MapNode.h"

#include "../Utils/NameIdMapping.h"

#include "Audio/Sound.h"

#include "Graphic/Resource/Color.h"
#include "Graphic/Effect/Lighting.h"
#include "Graphic/Scene/Hierarchy/BoundingBox.h"
#include "Graphic/Scene/Mesh/Mesh.h"

#include "Mathematic/Algebra/Vector3.h"

// PROTOCOL_VERSION >= 37
static const uint8_t CONTENTFEATURES_VERSION = 13;

class VisualEnvironment;
class BaseItemManager;
class BaseTextureSource;
class BaseShaderSource;
class NodeResolver;
class NodeManager;
class Map;

struct MapNode;
class MapNodeMetadata;


enum ContentParamType
{
    CPT_NONE,
    CPT_LIGHT,
};

enum ContentParamType2
{
    CPT2_NONE,
    // Need 8-bit param2
    CPT2_FULL,
    // Flowing liquid properties
    CPT2_FLOWINGLIQUID,
    // Direction for chests and furnaces and such
    CPT2_FACEDIR,
    // Direction for signs, torches and such
    CPT2_WALLMOUNTED,
    // Block level like FLOWINGLIQUID
    CPT2_LEVELED,
    // 2D rotation
    CPT2_DEGROTATE,
    // Mesh options for plants
    CPT2_MESHOPTIONS,
    // Index for palette
    CPT2_COLOR,
    // 3 bits of palette index, then facedir
    CPT2_COLORED_FACEDIR,
    // 5 bits of palette index, then wallmounted
    CPT2_COLORED_WALLMOUNTED,
    // Glasslike framed drawType internal liquid level, param2 values 0 to 63
    CPT2_GLASSLIKE_LIQUID_LEVEL,
    // 3 bits of palette index, then degrotate
    CPT2_COLORED_DEGROTATE,
};

enum LiquidType
{
    LIQUID_NONE,
    LIQUID_FLOWING,
    LIQUID_SOURCE,
};

enum NodeBoxType
{
    NODEBOX_REGULAR, // Regular block; allows buildableTo
    NODEBOX_FIXED, // Static separately defined box(es)
    NODEBOX_WALLMOUNTED, // Box for wall mounted nodes; (top, bottom, side)
    NODEBOX_LEVELED, // Same as fixed, but with dynamic height from param2. for snow, ...
    NODEBOX_CONNECTED, // optionally draws nodeboxes if a neighbor node attaches
};


struct NodeBox
{
    enum NodeBoxType type;
    // NODEBOX_REGULAR (no parameters)
    // NODEBOX_FIXED
    std::vector<BoundingBox<float>> fixed;
    // NODEBOX_WALLMOUNTED
    BoundingBox<float> wallTop;
    BoundingBox<float> wallBottom;
    BoundingBox<float> wallSide; // being at the -X side
    // NODEBOX_CONNECTED
    std::vector<BoundingBox<float>> connectTop;
    std::vector<BoundingBox<float>> connectBottom;
    std::vector<BoundingBox<float>> connectFront;
    std::vector<BoundingBox<float>> connectLeft;
    std::vector<BoundingBox<float>> connectBack;
    std::vector<BoundingBox<float>> connectRight;
    std::vector<BoundingBox<float>> disconnectedTop;
    std::vector<BoundingBox<float>> disconnectedBottom;
    std::vector<BoundingBox<float>> disconnectedFront;
    std::vector<BoundingBox<float>> disconnectedLeft;
    std::vector<BoundingBox<float>> disconnectedBack;
    std::vector<BoundingBox<float>> disconnectedRight;
    std::vector<BoundingBox<float>> disconnected;
    std::vector<BoundingBox<float>> disconnectedSides;

    NodeBox()
    {
        Reset();
    }

    void Reset();
    void Serialize(std::ostream &os) const;
    void Deserialize(std::istream &is);
};

enum LeavesStyle 
{
    LEAVES_FANCY,
    LEAVES_SIMPLE,
    LEAVES_OPAQUE,
};

enum AutoScale : unsigned char 
{
    AUTOSCALE_DISABLE,
    AUTOSCALE_ENABLE,
    AUTOSCALE_FORCE,
};

enum WorldAlignMode : unsigned char 
{
    WORLDALIGN_DISABLE,
    WORLDALIGN_ENABLE,
    WORLDALIGN_FORCE,
    WORLDALIGN_FORCE_NODEBOX,
};

class TextureSettings 
{
public:
    LeavesStyle mLeavesStyle;
    WorldAlignMode mWorldAlignedMode;
    AutoScale mAutoscaleMode;
    int mNodeTextureSize;
    bool mOpaqueWater;
    bool mConnectedGlass;
    bool mEnableMeshCache;
    bool mEnableMinimap;

    TextureSettings() = default;

    void ReadSettings();
};

enum NodeDrawType
{
    // A basic solid block
    NDT_NORMAL,
    // Nothing is drawn
    NDT_AIRLIKE,
    // Do not draw face towards same kind of flowing/source liquid
    NDT_LIQUID,
    // A very special kind of thing
    NDT_FLOWINGLIQUID,
    // Glass-like, don't draw faces towards other glass
    NDT_GLASSLIKE,
    // Leaves-like, draw all faces no matter what
    NDT_ALLFACES,
    // Enabled -> ndt_allfaces, disabled -> ndt_normal
    NDT_ALLFACES_OPTIONAL,
    // Single plane perpendicular to a surface
    NDT_TORCHLIKE,
    // Single plane parallel to a surface
    NDT_SIGNLIKE,
    // 2 vertical planes in a 'X' shape diagonal to XZ axes.
    // paramtype2 = "meshoptions" allows various forms, sizes and
    // vertical and horizontal random offsets.
    NDT_PLANTLIKE,
    // Fenceposts that connect to neighbouring fenceposts with horizontal bars
    NDT_FENCELIKE,
    // Selects appropriate junction texture to connect like rails to
    // neighbouring raillikes.
    NDT_RAILLIKE,
    // Custom script-definable structure of multiple cuboids
    NDT_NODEBOX,
    // Glass-like, draw connected frames and all visible faces.
    // param2 > 0 defines 64 levels of internal liquid
    // Uses 3 textures, one for frames, second for faces,
    // optional third is a 'special tile' for the liquid.
    NDT_GLASSLIKE_FRAMED,
    // Draw faces slightly rotated and only on neighbouring nodes
    NDT_FIRELIKE,
    // Enabled -> ndt_glasslike_framed, disabled -> ndt_glasslike
    NDT_GLASSLIKE_FRAMED_OPTIONAL,
    // Uses static meshes
    NDT_MESH,
    // Combined plantlike-on-solid
    NDT_PLANTLIKE_ROOTED
};

// Mesh options for NDT_PLANTLIKE with CPT2_MESHOPTIONS
static const unsigned char MO_MASK_STYLE = 0x07;
static const unsigned char MO_BIT_RANDOM_OFFSET = 0x08;
static const unsigned char MO_BIT_SCALE_SQRT2 = 0x10;
static const unsigned char MO_BIT_RANDOM_OFFSET_Y = 0x20;

enum PlantlikeStyle 
{
    PLANT_STYLE_CROSS,
    PLANT_STYLE_CROSS2,
    PLANT_STYLE_STAR,
    PLANT_STYLE_HASH,
    PLANT_STYLE_HASH2,
};

enum AlphaMode : unsigned char 
{
    ALPHAMODE_BLEND,
    ALPHAMODE_CLIP,
    ALPHAMODE_OPAQUE,
    ALPHAMODE_LEGACY_COMPAT, /* means either opaque or clip */
};

// Defines the number of special tiles per nodeMgr
//
// NOTE: When changing this value, the enum entries of OverrideTarget and
//       parser in TextureOverrideSource must be updated so that all special
//       tiles can be overridden.
#define CF_SPECIAL_COUNT 6


extern std::map<std::string, unsigned int> ContentParamTypes;

extern std::map<std::string, unsigned int> ContentParamType2s;

extern std::map<std::string, unsigned int> LiquidTypes;

extern std::map<std::string, unsigned int> NodeBoxTypes;

extern std::map<std::string, unsigned int> NodeTypes;

extern std::map<std::string, unsigned int> AlphaModes;


struct ContentFeatures
{
    /*
        Cached stuff
     */
     // 0     1     2     3     4     5
     // up    down  right left  back  front
    TileSpec tiles[6];
    // Special tiles
    // - Currently used for flowing liquids
    TileSpec specialTiles[CF_SPECIAL_COUNT];
    unsigned char solidness; // Used when choosing which face is drawn
    unsigned char visualSolidness; // When solidness=0, this tells how it looks like
    bool backfaceCulling;

    // Logic-side cached callback existence for fast skipping
    bool hasOnConstruct;
    bool hasOnDestruct;
    bool hasAfterDestruct;

    /*
        Actual data
     */

     // --- GENERAL PROPERTIES ---

    std::string name; // "" = unodeMgrined node

    std::vector<std::pair<std::string, std::string>> drops; // any node drop?

    std::unordered_map<std::string, int> groups;
    // Type of MapNode::param1
    ContentParamType paramType;
    // Type of MapNode::param2
    ContentParamType2 paramType2;

    // --- VISUAL PROPERTIES ---

    enum NodeDrawType drawType;
    std::string mesh;

    std::shared_ptr<BaseMesh> visualMesh[24];
    SColor minimapColor;

    float visualScale; // Misc. scale parameter
    Tile tile[6];
    // These will be drawn over the base tiles.
    Tile tileOverlay[6];
    Tile tileSpecial[CF_SPECIAL_COUNT]; // eg. flowing liquid
    AlphaMode alpha;
    // The color of the node.
    SColor color;
    std::string paletteName;
    std::vector<SColor>* palette;
    // Used for waving leaves/plants
    unsigned char waving;
    // for NDT_CONNECTED pairing
    unsigned char connectSides;
    std::vector<std::string> connectsTo;
    std::vector<uint16_t> connectsToIds;
    // Post effect color, drawn when the camera is inside the node.
    SColor postEffectColor;
    // Flowing liquid or leveled nodebox, value = default level
    unsigned char leveled;
    // Maximum value for leveled nodes
    unsigned char leveledMax;

    // --- LIGHTING-RELATED ---

    bool lightPropagates;
    bool sunlightPropagates;
    // Amount of light the node emits
    unsigned char lightSource;

    // --- MAP GENERATION ---

    // True for all ground-like things like stone and mud, false for eg. trees
    bool isGroundContent;

    // --- INTERACTION PROPERTIES ---

    // This is used for collision detection.
    // Also for general solidness queries.
    bool walkable;
    // Player can point to these
    bool pointable;
    // Player can dig these
    bool diggable;
    // Player can climb these
    bool climbable;
    // Player can build on these
    bool buildableTo;
    // Player cannot build to these (placement prediction disabled)
    bool rightClickable;
    unsigned int damagePerSecond;
    // visual dig prediction
    std::string nodeDigPrediction;

    // --- LIQUID PROPERTIES ---

    // Whether the node is non-liquid, source liquid or flowing liquid
    enum LiquidType liquidType;
    // If the content is liquid, this is the flowing version of the liquid.
    std::string liquidAlternativeFlowing;
    unsigned int liquidAlternativeFlowingId;
    // If the content is liquid, this is the source version of the liquid.
    std::string liquidAlternativeSource;
    unsigned int liquidAlternativeSourceId;
    // Viscosity for fluid flow, ranging from 1 to 7, with
    // 1 giving almost instantaneous propagation and 7 being
    // the slowest possible
    unsigned char liquidViscosity;
    // Is liquid renewable (new liquid source will be created between 2 existing)
    bool liquidRenewable;
    // Number of flowing liquids surrounding source
    unsigned char liquidRange;
    unsigned char drowning;
    // Liquids flow into and replace node
    bool floodable;

    // --- NODEBOXES ---

    NodeBox nodeBox;
    NodeBox selectionBox;
    NodeBox collisionBox;

    // --- SOUND PROPERTIES ---

    SimpleSound soundFootstep;
    SimpleSound soundPlace;
    SimpleSound soundDig;
    SimpleSound soundDug;

    // --- LEGACY ---

    // Compatibility with old maps
    // Set to true if paramtype used to be 'facedir_simple'
    bool legacyFacedirSimple;
    // Set to true if wall_mounted used to be set to true
    bool legacyWallmounted;

    /*
        Methods
    */

    ContentFeatures();
    ~ContentFeatures();
    void Reset();
    void Serialize(std::ostream &os) const;
    void Deserialize(std::istream &is);

    /*
        Some handy methods
    */
    void SetDefaultAlphaMode()
    {
        switch (drawType) 
        {
            case NDT_NORMAL:
            case NDT_LIQUID:
            case NDT_FLOWINGLIQUID:
                alpha = ALPHAMODE_OPAQUE;
                break;
            case NDT_NODEBOX:
            case NDT_MESH:
                alpha = ALPHAMODE_LEGACY_COMPAT; // this should eventually be OPAQUE
                break;
            default:
                alpha = ALPHAMODE_CLIP;
                break;
        }
    }

    bool NeedsBackfaceCulling() const
    {
        switch (drawType) 
        {
            case NDT_TORCHLIKE:
            case NDT_SIGNLIKE:
            case NDT_FIRELIKE:
            case NDT_RAILLIKE:
            case NDT_PLANTLIKE:
            case NDT_PLANTLIKE_ROOTED:
            case NDT_MESH:
                return false;
            default:
                return true;
        }
    }

    bool IsLiquid() const 
    {
        return (liquidType != LIQUID_NONE);
    }

    bool SameLiquid(const ContentFeatures& f) const 
    {
        if (!IsLiquid() || !f.IsLiquid()) 
            return false;
        return (liquidAlternativeFlowingId == f.liquidAlternativeFlowingId);
    }

    int GetGroup(const std::string& group) const
    {
        std::unordered_map<std::string, int>::const_iterator i = groups.find(group);
        if (i == groups.end())
            return 0;
        return i->second;
    }

    void UpdateTextures(BaseTextureSource* textureSrc, 
        BaseShaderSource* shaderSrc, const TextureSettings& texSettings);

private:

    /*
     * Checks if any tile texture has any transparent pixels.
     * Prints a warning and returns true if that is the case, false otherwise.
     * This is supposed to be used for use_texture_alpha backwards compatibility.
     */
    bool TextureAlphaCheck(BaseTextureSource* textureSrc, const Tile* tiles, int length);

    void SetAlphaFromLegacy(unsigned char legacyAlpha);

    unsigned char GetAlphaForLegacy() const;
};

/*
 * @brief This class is for getting the actual properties of nodes from their
 * content ID.
 *
 * @details The nodes on the map are represented by three numbers (see MapNode).
 * The first number (param0) is the type of a node. All node types have own
 * properties (see ContentFeatures). This class is for storing and getting the
 * properties of nodes.
 * The manager is first filled with registered nodes, then as the game begins,
 * functions only get `const` pointers to it, to prevent modification of
 * registered nodes.
 */
class NodeManager 
{
public:
    /*!
     * Creates a NodeManager, and registers three ContentFeatures:
     * \ref CONTENT_AIR, \ref CONTENT_UNKNOWN and \ref CONTENT_IGNORE.
     */
    NodeManager();
    ~NodeManager();

    /*!
     * Returns the properties for the given content type.
     * @param c content type of a node
     * @return properties of the given content type, or \ref CONTENT_UNKNOWN
     * if the given content type is not registered.
     */
    inline const ContentFeatures& Get(uint16_t content) const 
    {
        return content < mContentFeatures.size() ?
            mContentFeatures[content] : mContentFeatures[CONTENT_UNKNOWN];
    }

    /*!
     * Returns the properties of the given node.
     * @param n a map node
     * @return properties of the given node or @ref CONTENT_UNKNOWN if the
     * given content type is not registered.
     */
    inline const ContentFeatures& Get(const MapNode& node) const 
    {
        return Get(node.GetContent());
    }

    /*!
     * Returns the node properties for a node name.
     * @param name name of a node
     * @return properties of the given node or @ref CONTENT_UNKNOWN if
     * not found
     */
    const ContentFeatures& Get(const std::string& name) const;

    /*!
     * Returns the content ID for the given name.
     * @param name a node name
     * @param[out] result will contain the content ID if found, otherwise
     * remains unchanged
     * @return true if the ID was found, false otherwise
     */
    bool GetId(const std::string& name, uint16_t& result) const;

    /*!
     * Returns the content ID for the given name.
     * @param name a node name
     * @return ID of the node or @ref CONTENT_IGNORE if not found
     */
    uint16_t GetId(const std::string& name) const;

    /*!
     * Returns the content IDs of the given node name or node group name.
     * Group names start with "group:".
     * @param name a node name or node group name
     * @param[out] result will be appended with matching IDs
     * @return true if `name` is a valid node name or a (not necessarily
     * valid) group name
     */
    bool GetIds(const std::string& name, std::vector<uint16_t>& result) const;

    /*!
     * Returns the smallest box in integer node coordinates that
     * contains all nodes' selection boxes. The returned box might be larger
     * than the minimal size if the largest node is removed from the manager.
     */
    inline BoundingBox<short> GetSelectionBoxIntUnion() const 
    {
        return mSelectionBoxIntUnion;
    }

    /*!
     * Checks whether a node connects to an adjacent node.
     * @param from the node to be checked
     * @param to the adjacent node
     * @param connectFace a bit field indicating which face of the node is
     * adjacent to the other node.
     * Bits: +y (least significant), -y, -z, -x, +z, +x (most significant).
     * @return true if the node connects, false otherwise
     */
    bool NodeboxConnects(MapNode from, MapNode to, unsigned char connectFace) const;

    /*!
     * Registers a NodeResolver to wait for the registration of
     * ContentFeatures. Once the node registration finishes, all
     * listeners are notified.
     */
    void PendNodeResolve(NodeResolver *nr) const;

    /*!
     * Stops listening to the NodeManager.
     * @return true if the listener was registered before, false otherwise
     */
    bool CancelNodeResolveCallback(NodeResolver *nr) const;

    /*!
     * Registers a new node type with the given name and allocates a new
     * content ID.
     * Should not be called with an already existing name.
     * @param name name of the node, must match with `def.name`.
     * @param cFeatures definition of the registered node type.
     * @return ID of the registered node or @ref CONTENT_IGNORE if
     * the function could not allocate an ID.
     */
    uint16_t Set(const std::string& name, const ContentFeatures& cFeatures);

    /*!
     * Allocates a blank node ID for the given name.
     * @param name name of a node
     * @return allocated ID or @ref CONTENT_IGNORE if could not allocate
     * an ID.
     */
    uint16_t AllocateDummy(const std::string& name);

    /*!
     * Removes the given node name from the manager.
     * The node ID will remain in the manager, but won't be linked to any name.
     * @param name name to be removed
     */
    void RemoveNode(const std::string& name);

    /*!
     * Regenerates the alias list (a map from names to node IDs).
     * @param itemMgr the item definition manager containing alias information
     */
    void UpdateAliases(BaseItemManager* itemMgr);

    /*!
     * Replaces the textures of registered nodes with the ones specified in
     * the texturepack's override.txt file
     *
     * @param overrides the texture overrides
     */
    void ApplyTextureOverrides(const std::vector<TextureOverride>& overrides);

    /*!
     * Only the visual uses this. Loads textures and shaders required for
     * rendering the nodes.
     * @param VisualEnvironment.
     * @param progressCbk called each time a node is loaded. Arguments:
     * `progressCbkArgs`, number of loaded ContentFeatures, number of
     * total ContentFeatures.
     * @param progressCbkArgs passed to the callback function
     */
    void UpdateTextures(VisualEnvironment* env,
        void(*progressCbk)(void* progressArgs, unsigned int progress, unsigned int maxProgress),
        void* progressCbkArgs);

    /*!
     * Writes the content of this manager to the given output stream.
     */
    void Serialize(std::ostream& os) const;

    /*!
     * Restores the manager from a Serialized stream.
     * This clears the previous state.
     * @param is input stream containing a Serialized NodeManager
     */
    void Deserialize(std::istream& is);

    /*!
     * Used to indicate that node registration has finished.
     * @param completed tells whether registration is complete
     */
    inline void SetNodeRegistrationStatus(bool completed) 
    {
        mNodeRegistrationComplete = completed;
    }

    /*!
     * Notifies the registered NodeResolver instances that node registration
     * has finished, then unregisters all listeners.
     * Must be called after node registration has finished!
     */
    void RunNodeResolveCallbacks();

    /*!
     * Sets the registration completion flag to false and unregisters all
     * NodeResolver instances listening to the manager.
     */
    void ResetNodeResolveState();

    /*!
     * Resolves (caches the IDs) cross-references between nodes,
     * like liquid alternatives.
     * Must be called after node registration has finished!
     */
    void ResolveCrossrefs();

private:
    /*!
     * Resets the manager to its initial state.
     * See the documentation of the constructor.
     */
    void Clear();

    /*!
     * Allocates a new content ID, and returns it.
     * @return the allocated ID or \ref CONTENT_IGNORE if could not allocate
     */
    uint16_t AllocateId();

    /*!
     * Binds the given content ID and node name.
     * Registers them in \ref mNameId and
     * \ref mNameIdWithAliases.
     * @param i a content ID
     * @param name a node name
     */
    void AddNameIdMapping(uint16_t i, const std::string& name);

    /*!
     * Removes a content ID from all groups.
     * Erases content IDs from vectors in \ref mGroupToItems and
     * removes empty vectors.
     * @param id Content ID
     */
    void EraseIdFromGroups(uint16_t id);

    /*!
     * Recalculates mSelectionBoxIntUnion based on
     * mSelectionBoxUnion.
     */
    void FixSelectionBoxIntUnion();

    //! Features indexed by ID.
    std::vector<ContentFeatures> mContentFeatures;

    //! A mapping for fast conversion between names and IDs
    NameIdMapping mNameId;

    /*!
     * Like @ref mNameId, but maps only from names to IDs, and
     * includes aliases too. Updated by \ref updateAliases().
     * Note: Not Serialized.
     */
    std::unordered_map<std::string, uint16_t> mNameIdWithAliases;

    /*!
     * A mapping from group names to a vector of content types that belong
     * to it. Necessary for a direct lookup in \ref getIds().
     * Note: Not Serialized.
     */
    std::unordered_map<std::string, std::vector<uint16_t>> mGroupToItems;

    /*!
     * The next ID that might be free to allocate.
     * It can be allocated already, because \ref CONTENT_AIR,
     * \ref CONTENT_UNKNOWN and \ref CONTENT_IGNORE are registered when the
     * manager is initialized, and new IDs are allocated from 0.
     */
    uint16_t mNextId;

    //! True if all nodes have been registered.
    bool mNodeRegistrationComplete;

    /*!
     * The union of all nodes' selection boxes.
     * Might be larger if big nodes are removed from the manager.
     */
    BoundingBox<float> mSelectionBoxUnion;

    /*!
     * The smallest box in integer node coordinates that
     * contains all nodes' selection boxes.
     * Might be larger if big nodes are removed from the manager.
     */
    BoundingBox<short> mSelectionBoxIntUnion;

    /*!
     * NodeResolver instances to notify once node registration has finished.
     * Even constant NodeManager instances can register listeners.
     */
    mutable std::vector<NodeResolver*> mPendingResolveCallbacks;
};

std::shared_ptr<NodeManager> CreateNodeManager();

// NodeResolver: Queue for node names which are then translated
// to uint16_t after the NodeManager was initialized
class NodeResolver 
{

public:
    NodeResolver();
    virtual ~NodeResolver();
    // Callback which is run as soon NodeManager is ready
    virtual void ResolveNodeNames() = 0;

    // required because this class is used as mixin for ObjDef
    void CloneTo(NodeResolver* res) const;

    bool GetIdFromNrBacklog(uint16_t* resultOut,
        const std::string& nodeAlt, uint16_t cFallback, bool errorOnFallback = true);
    bool GetIdsFromNrBacklog(std::vector<uint16_t>* resultOut,
        bool allRequired = false, uint16_t cFallback = CONTENT_IGNORE);

    inline bool IsResolveDone() const { return mResolveDone; }
    void Reset(bool resolveDone = false);

    // Vector containing all node names in the resolve "queue"
    std::vector<std::string> mNodeNames;
    // Specifies the "set size" of node names which are to be processed
    // this is used for GetIdsFromNrBacklog
    // TODO: replace or remove
    std::vector<size_t> mNodeListSizes;

protected:

    friend class NodeManager; // mNodeManager

    const NodeManager* mNodeManager = nullptr;
    // Index of the next "mNodeNames" entry to resolve
    unsigned int mNodeNamesIdx = 0;

private:

    void NodeResolveInternal();

    // Index of the next "mNodeListSizes" entry to process
    unsigned int mNodeListSizesIdx = 0;
    bool mResolveDone = false;
};



/*
    NodeTimer provides per-node timed callback functionality.
    Can be used for:
    - Furnaces, to keep the fire burnin'
    - "activated" nodes that snap back to their original state
      after a fixed amount of time (mesecons buttons, for example)
*/

class NodeTimer
{
public:
    NodeTimer() = default;
    NodeTimer(const Vector3<short>& position) : mPosition(position) {}
    NodeTimer(float timeout, float elapsed, Vector3<short> position) :
        mTimeout(timeout), mElapsed(elapsed), mPosition(position) {}
    ~NodeTimer() = default;

    void Serialize(std::ostream& os) const;
    void Deserialize(std::istream& is);

    float mTimeout = 0.0f;
    float mElapsed = 0.0f;
    Vector3<short> mPosition;
};

/*
    List of timers of all the nodes of a block
*/

class NodeTimerList
{
public:
    NodeTimerList() = default;
    ~NodeTimerList() = default;

    void Serialize(std::ostream& os, uint8_t mapFormatVersion) const;
    void Deserialize(std::istream& is, uint8_t mapFormatVersion);

    // Get timer
    NodeTimer Get(const Vector3<short>& pos)
    {
        std::map<Vector3<short>, std::multimap<double, NodeTimer>::iterator>::iterator n = mIterators.find(pos);
        if (n == mIterators.end())
            return NodeTimer();
        NodeTimer t = n->second->second;
        t.mElapsed = t.mTimeout - (float)(n->second->first - mTime);
        return t;
    }
    // Deletes timer
    void Remove(Vector3<short> pos) 
    {
        std::map<Vector3<short>, std::multimap<double, NodeTimer>::iterator>::iterator n = mIterators.find(pos);
        if (n != mIterators.end()) 
        {
            double removedTime = n->second->first;
            mTimers.erase(n->second);
            mIterators.erase(n);
            // Yes, this is float equality, but it is not a problem
            // since we only test equality of floats as an ordered type
            // and thus we never lose precision
            if (removedTime == mNextTriggerTime) 
            {
                if (mTimers.empty())
                    mNextTriggerTime = -1.;
                else
                    mNextTriggerTime = mTimers.begin()->first;
            }
        }
    }
    // UnodeMgrined behaviour if there already is a timer
    void Insert(NodeTimer timer) 
    {
        Vector3<short> p = timer.mPosition;
        double triggerTime = mTime + (double)(timer.mTimeout - timer.mElapsed);
        std::multimap<double, NodeTimer>::iterator it =
            mTimers.insert(std::pair<double, NodeTimer>(triggerTime, timer));
        mIterators.insert(
            std::pair<Vector3<short>, std::multimap<double, NodeTimer>::iterator>(p, it));
        if (mNextTriggerTime == -1. || triggerTime < mNextTriggerTime)
            mNextTriggerTime = triggerTime;
    }
    // Deletes old timer and sets a new one
    inline void Set(const NodeTimer& timer) 
    {
        Remove(timer.mPosition);
        Insert(timer);
    }
    // Deletes all timers
    void Clear() 
    {
        mTimers.clear();
        mIterators.clear();
        mNextTriggerTime = -1.;
    }

    // Move forward in time, returns elapsed timers
    std::vector<NodeTimer> Step(float dTime);

private:
    std::multimap<double, NodeTimer> mTimers;
    std::map<Vector3<short>, std::multimap<double, NodeTimer>::iterator> mIterators;
    double mNextTriggerTime = -1.0;
    double mTime = 0.0;
};

#endif
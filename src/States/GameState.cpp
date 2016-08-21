#include "GameState.h"
#include "PlanetSelectState.h"

#include "../Constants.h"
#include "../GraphWalker.h"
#include "../StructureFactory.h"
#include "../StructureTranslator.h"

#include "../Map/Tile.h"

#include "../Things/Robots/Robots.h"
#include "../Things/Structures/Structures.h"

#include <sstream>
#include <vector>

// Disable some warnings that can be safely ignored.
#pragma warning(disable: 4244) // possible loss of data (floats to int and vice versa)


using namespace std;
using namespace NAS2D;


const std::string	MAP_TERRAIN_EXTENSION		= "_a.png";
const std::string	MAP_DISPLAY_EXTENSION		= "_b.png";

const int MAX_TILESET_INDEX	= 4;


Rectangle_2d MENU_ICON;


std::string					CURRENT_LEVEL_STRING;
std::map <int, std::string>	LEVEL_STRING_TABLE;

/**
 * C'Tor
 */
GameState::GameState(const string& _m, const string& _t, int _d, int _mc, AiVoiceNotifier::AiGender _g) :	mFont("fonts/mig6800_8x16.png", 8, 16, 0),
																											mTinyFont("fonts/ui-normal.png", 7, 9, -1),
																											mTileMap(new TileMap(_m, _t, _d, _mc)),
																											mMapDisplay(_m + MAP_DISPLAY_EXTENSION),
																											mHeightMap(_m + MAP_TERRAIN_EXTENSION),
																											mUiIcons("ui/icons.png"),
																											mAiVoiceNotifier(_g),
																											//mBgMusic("music/track_01.ogg"),
																											mCurrentPointer(POINTER_NORMAL),
																											mCurrentStructure(SID_NONE),
																											mDiggerDirection(mTinyFont),
																											mFactoryProduction(mTinyFont),
																											mStructureInspector(mTinyFont),
																											mTileInspector(mTinyFont),
																											mInsertMode(INSERT_NONE),
																											mTurnCount(0),
																											mCurrentMorale(600),
																											mReturnState(NULL),
																											mLeftButtonDown(false),
																											mDebug(false)
{}


/**
 * D'Tor
 */
GameState::~GameState()
{
	scrubRobotList();
	delete mTileMap;

	EventHandler& e = Utility<EventHandler>::get();
	e.activate().Disconnect(this, &GameState::onActivate);
	e.keyDown().Disconnect(this, &GameState::onKeyDown);
	e.mouseButtonDown().Disconnect(this, &GameState::onMouseDown);
	e.mouseButtonUp().Disconnect(this, &GameState::onMouseUp);
	e.mouseMotion().Disconnect(this, &GameState::onMouseMove);
}


/**
 * Initialize values, the UI and set up event handling.
 */
void GameState::initialize()
{
	mReturnState = this;

	EventHandler& e = Utility<EventHandler>::get();

	e.activate().Connect(this, &GameState::onActivate);

	e.keyDown().Connect(this, &GameState::onKeyDown);

	e.mouseButtonDown().Connect(this, &GameState::onMouseDown);
	e.mouseButtonUp().Connect(this, &GameState::onMouseUp);
	e.mouseMotion().Connect(this, &GameState::onMouseMove);
	e.mouseWheel().Connect(this, &GameState::onMouseWheel);

	// UI
	initUi();

	MENU_ICON(Utility<Renderer>::get().width() - constants::MARGIN_TIGHT * 2 - constants::RESOURCE_ICON_SIZE, 0, constants::RESOURCE_ICON_SIZE + constants::MARGIN_TIGHT * 2, constants::RESOURCE_ICON_SIZE + constants::MARGIN_TIGHT * 2);

	mPointers.push_back(Pointer("ui/pointers/normal.png", 0, 0));
	mPointers.push_back(Pointer("ui/pointers/place_tile.png", 16, 16));
	mPointers.push_back(Pointer("ui/pointers/inspect.png", 8, 8));

	mPlayerResources.capacity(constants::BASE_STORAGE_CAPACITY);

	Utility<Renderer>::get().fadeIn(constants::FADE_SPEED);

	CURRENT_LEVEL_STRING = constants::LEVEL_SURFACE;

	//Utility<Mixer>::get().fadeInMusic(mBgMusic);

	if (LEVEL_STRING_TABLE.empty())
	{
		LEVEL_STRING_TABLE[constants::DEPTH_SURFACE] = constants::LEVEL_SURFACE;
		LEVEL_STRING_TABLE[constants::DEPTH_UNDERGROUND_1] = constants::LEVEL_UG1;
		LEVEL_STRING_TABLE[constants::DEPTH_UNDERGROUND_2] = constants::LEVEL_UG2;
		LEVEL_STRING_TABLE[constants::DEPTH_UNDERGROUND_3] = constants::LEVEL_UG3;
		LEVEL_STRING_TABLE[constants::DEPTH_UNDERGROUND_4] = constants::LEVEL_UG4;
	}
}


/**
 * Updates the entire state of the game.
 */
State* GameState::update()
{
	Renderer& r = Utility<Renderer>::get();

	//r.drawImageStretched(mBackground, 0, 0, r.width(), r.height());
	r.drawBoxFilled(0, 0, r.width(), r.height(), 25, 25, 25);

	mTileMap->injectMouse(mMousePosition.x(), mMousePosition.y());
	mTileMap->draw();
	drawUI();

	r.drawText(mFont, CURRENT_LEVEL_STRING, r.width() - mFont.width(CURRENT_LEVEL_STRING) - 5, mMiniMapBoundingBox.y() - mFont.height() - 10, 255, 255, 255);

	if(mDebug) drawDebug();

	if (r.isFading())
		return this;

	return mReturnState;
}


void GameState::drawMiniMap()
{
	Renderer& r = Utility<Renderer>::get();

	if(mBtnToggleHeightmap.toggled())
		r.drawImage(mHeightMap, mMiniMapBoundingBox.x(), mMiniMapBoundingBox.y());
	else
		r.drawImage(mMapDisplay, mMiniMapBoundingBox.x(), mMiniMapBoundingBox.y());

	if (mCCLocation.x() != 0 && mCCLocation.y() != 0)
	{
		// fixme: find a more efficient way to do this.
		int xClip = clamp(15 - mCCLocation.x(), 0, 15);
		int yClip = clamp(15 - mCCLocation.y(), 0, 15);
		int wClip = clamp((mCCLocation.x() + 15) - mMiniMapBoundingBox.w(), 0, 15);
		int hClip = clamp((mCCLocation.y() + 15) - mMiniMapBoundingBox.h(), 0, 15);;

		r.drawSubImage(mUiIcons, mCCLocation.x() + mMiniMapBoundingBox.x() - 15 + xClip, mCCLocation.y() + mMiniMapBoundingBox.y() - 15 + yClip, xClip, yClip + 5, 30 - xClip - wClip, 30 - yClip - hClip);
		r.drawBoxFilled(mCCLocation.x() + mMiniMapBoundingBox.x() - 1, mCCLocation.y() + mMiniMapBoundingBox.y() - 1, 3, 3, 255, 255, 255);
	}


	for(size_t i = 0; i < mTileMap->mineLocations().size(); i++)
	{
		if (mTileMap->getTile(mTileMap->mineLocations()[i].x(), mTileMap->mineLocations()[i].y(), 0)->mine()->active())
			r.drawSubImage(mUiIcons, mTileMap->mineLocations()[i].x() + mMiniMapBoundingBox.x() - 2, mTileMap->mineLocations()[i].y() + mMiniMapBoundingBox.y() - 2, 8.0f, 0.0f, 5.0f, 5.0f);
		else
			r.drawSubImage(mUiIcons, mTileMap->mineLocations()[i].x() + mMiniMapBoundingBox.x() - 2, mTileMap->mineLocations()[i].y() + mMiniMapBoundingBox.y() - 2, 0.0f, 0.0f, 5.0f, 5.0f);
	}

	for (auto it = mRobotList.begin(); it != mRobotList.end(); ++it)
		r.drawPixel(it->second->x()  + mMiniMapBoundingBox.x(), it->second->y() + mMiniMapBoundingBox.y(), 0, 255, 255);

	//r.drawBox(mMiniMapBoundingBox, 0, 0, 0);

	r.drawBox(mMiniMapBoundingBox.x() + mTileMap->mapViewLocation().x() + 1, mMiniMapBoundingBox.y() + mTileMap->mapViewLocation().y() + 1, mTileMap->edgeLength(), mTileMap->edgeLength(), 0, 0, 0, 180);
	r.drawBox(mMiniMapBoundingBox.x() + mTileMap->mapViewLocation().x(), mMiniMapBoundingBox.y() + mTileMap->mapViewLocation().y(), mTileMap->edgeLength(), mTileMap->edgeLength(), 255, 255, 255);
}


int GameState::foodInStorage()
{
	int food_count = 0;

	auto sl = mStructureManager.structureList(Structure::STRUCTURE_FOOD_PRODUCTION);
	for (size_t i = 0; i < sl.size(); ++i)
	{
		if (sl[i]->operational() || sl[i]->isIdle())
			food_count += sl[i]->storage().food();
	}

	return food_count;
}


int GameState::foodTotalStorage()
{
	int food_storage = 0;

	auto sl = mStructureManager.structureList(Structure::STRUCTURE_FOOD_PRODUCTION);
	for (size_t i = 0; i < sl.size(); ++i)
	{
		if (sl[i]->operational() || sl[i]->isIdle())
			food_storage += AGRIDOME_CAPACITY;
	}

	return food_storage;
}


void GameState::drawResourceInfo()
{
	Renderer& r = Utility<Renderer>::get();

	r.drawBoxFilled(0, 0, r.width(), constants::RESOURCE_ICON_SIZE + 4, 0, 0, 0);
	//r.drawBox(mResourceInfoBox, 0, 0, 0);

	// Resources
	int x = constants::MARGIN_TIGHT;
	int y = constants::MARGIN_TIGHT;

	int textY = 6;
	int offsetX = constants::RESOURCE_ICON_SIZE + 40;
	int margin = constants::RESOURCE_ICON_SIZE + constants::MARGIN;

	// Refined Resources
	r.drawSubImage(mUiIcons, x, y , 64, 16, constants::RESOURCE_ICON_SIZE, constants::RESOURCE_ICON_SIZE);
	r.drawText(mTinyFont, string_format("%i", mPlayerResources.commonMetals()), x + margin, textY, 255, 255, 255);

	r.drawSubImage(mUiIcons, x + offsetX, y, 80, 16, constants::RESOURCE_ICON_SIZE, constants::RESOURCE_ICON_SIZE);
	r.drawText(mTinyFont, string_format("%i", mPlayerResources.rareMetals()), (x + offsetX) + margin, textY, 255, 255, 255);

	r.drawSubImage(mUiIcons, (x + offsetX) * 2, y, 96, 16, constants::RESOURCE_ICON_SIZE, constants::RESOURCE_ICON_SIZE);
	r.drawText(mTinyFont, string_format("%i", mPlayerResources.commonMinerals()), (x + offsetX) * 2 + margin, textY, 255, 255, 255);

	r.drawSubImage(mUiIcons, (x + offsetX) * 3, y, 112, 16, constants::RESOURCE_ICON_SIZE, constants::RESOURCE_ICON_SIZE);
	r.drawText(mTinyFont, string_format("%i", mPlayerResources.rareMinerals()), (x + offsetX) * 3 + margin, textY, 255, 255, 255);

	// Storage Capacity
	r.drawSubImage(mUiIcons, (x + offsetX) * 4, y, 96, 32, constants::RESOURCE_ICON_SIZE, constants::RESOURCE_ICON_SIZE);
	r.drawText(mTinyFont, string_format("%i/%i", mPlayerResources.currentLevel(), mPlayerResources.capacity()), (x + offsetX) * 4 + margin, textY, 255, 255, 255);

	// Food
	r.drawSubImage(mUiIcons, (x + offsetX) * 6, y, 64, 32, constants::RESOURCE_ICON_SIZE, constants::RESOURCE_ICON_SIZE);
	r.drawText(mTinyFont, string_format("%i/%i", foodInStorage(), foodTotalStorage()), (x + offsetX) * 6 + margin, textY, 255, 255, 255);

	// Energy
	r.drawSubImage(mUiIcons, (x + offsetX) * 8, y, 80, 32, constants::RESOURCE_ICON_SIZE, constants::RESOURCE_ICON_SIZE);
	r.drawText(mTinyFont, string_format("%i/%i", mPlayerResources.energy(), mStructureManager.totalEnergyProduction()), (x + offsetX) * 8 + margin, textY, 255, 255, 255);
	
	// Population / Morale
	r.drawSubImage(mUiIcons, (x + offsetX) * 10, y, 176 + (mCurrentMorale / 200) * constants::RESOURCE_ICON_SIZE, 0, constants::RESOURCE_ICON_SIZE, constants::RESOURCE_ICON_SIZE);
	r.drawText(mTinyFont, "100", (x + offsetX) * 10 + margin, textY, 255, 255, 255);


	// Turns
	r.drawSubImage(mUiIcons, r.width() - 80, y, 128, 0, constants::RESOURCE_ICON_SIZE, constants::RESOURCE_ICON_SIZE);
	r.drawText(mTinyFont, string_format("%i", mTurnCount), r.width() - 80 + margin, textY, 255, 255, 255);

	// ugly
	if (isPointInRect(mMousePosition, MENU_ICON))
		r.drawSubImage(mUiIcons, MENU_ICON.x() + constants::MARGIN_TIGHT, MENU_ICON.y() + constants::MARGIN_TIGHT, 144, 32, constants::RESOURCE_ICON_SIZE, constants::RESOURCE_ICON_SIZE);
	else
		r.drawSubImage(mUiIcons, MENU_ICON.x() + constants::MARGIN_TIGHT, MENU_ICON.y() + constants::MARGIN_TIGHT, 128, 32, constants::RESOURCE_ICON_SIZE, constants::RESOURCE_ICON_SIZE);

	// Robots
	// Start from the bottom - The bottom UI Height - Icons Height - 8 (1 offset to avoid the last to be glued with at the border) 
	y = (int)r.height() - constants::BOTTOM_UI_HEIGHT - 25 - 8;
	textY = y + 10;	// Same position + 10 to center the text with the graphics
	margin = 28;	// Margin of 28 px from the graphics to the text
	x = 0; offsetX = 1;	// Start a the left side of the screen + an offset of 1 to detatch from the border
	// Miner (last one)
	r.drawSubImage(mUiIcons, (x + offsetX) * 8, y, 231, 18, 25, 25);
	r.drawText(mTinyFont, string_format("%i/%i", mRobotPool.getAvailableCount(ROBOT_MINER), mRobotPool.miners().size()), (x + offsetX) * 8 + margin, textY, 255, 255, 255);
	// Dozer (Midle one)
	textY -= 25; y -= 25;
	r.drawSubImage(mUiIcons, (x + offsetX) * 8, y, 206, 18, 25, 25);
	r.drawText(mTinyFont, string_format("%i/%i", mRobotPool.getAvailableCount(ROBOT_DOZER), mRobotPool.dozers().size()), (x + offsetX) * 8 + margin, textY, 255, 255, 255);
	// Digger (First one)
	textY -= 25; y -= 25;
	r.drawSubImage(mUiIcons, (x + offsetX) * 8, y, 181, 18, 25, 25);
	r.drawText(mTinyFont, string_format("%i/%i", mRobotPool.getAvailableCount(ROBOT_DIGGER),mRobotPool.diggers().size()), (x + offsetX) * 8 + margin, textY, 255, 255, 255);
}


void GameState::drawDebug()
{
	Renderer& r = Utility<Renderer>::get();
	
	r.drawText(mFont, string_format("FPS: %i", mFps.fps()), 10, 25, 255, 255, 255);
	r.drawText(mFont, string_format("Map Dimensions: %i, %i", mTileMap->width(), mTileMap->height()), 10, 25 + mFont.height(), 255, 255, 255);
	r.drawText(mFont, string_format("Max Digging Depth: %i", mTileMap->maxDepth()), 10, 25 + mFont.height() * 2, 255, 255, 255);
	r.drawText(mFont, string_format("Map Mouse Hover Coords: %i, %i", mTileMap->tileMouseHoverX(), mTileMap->tileMouseHoverY()), 10, 25 + mFont.height() * 3, 255, 255, 255);
	r.drawText(mFont, string_format("Current Depth: %i", mTileMap->currentDepth()), 10, 25 + mFont.height() * 4, 255, 255, 255);

	r.drawText(mFont, string_format("Structure Count: %i", mStructureManager.count()), 10, 25 + mFont.height() * 6, 255, 255, 255);
}


/**
 * Window activation handler.
 */
void GameState::onActivate(bool _b)
{
	if (!_b)
	{
		mLeftButtonDown = false;
	}
}


/**
 * Key down event handler.
 */
void GameState::onKeyDown(KeyCode key, KeyModifier mod, bool repeat)
{
	bool viewUpdated = false; // don't like flaggy code like this
	Point_2d pt = mTileMap->mapViewLocation();

	switch(key)
	{
		case KEY_w:
		case KEY_UP:
			viewUpdated = true;
			pt.y(clamp(--pt.y(), 0, mTileMap->height() - mTileMap->edgeLength()));
			break;

		case KEY_s:
		case KEY_DOWN:
			viewUpdated = true;
			pt.y(clamp(++pt.y(), 0, mTileMap->height() - mTileMap->edgeLength()));
			break;

		case KEY_a:
		case KEY_LEFT:
			viewUpdated = true;
			pt.x(clamp(--pt.x(), 0, mTileMap->width() - mTileMap->edgeLength()));
			break;

		case KEY_d:
		case KEY_RIGHT:
			viewUpdated = true;
			pt.x(clamp(++pt.x(), 0, mTileMap->width() - mTileMap->edgeLength()));
			break;

		case KEY_0:
			viewUpdated = true;
			changeDepth(0);
//			if(changeDepth(0))
//				updateCurrentLevelString(0);
			break;

		case KEY_1:
			viewUpdated = true;
			changeDepth(1);
			break;

		case KEY_2:
			viewUpdated = true;
			changeDepth(2);
			break;

		case KEY_3:
			viewUpdated = true;
			changeDepth(3);
			break;

		case KEY_4:
			viewUpdated = true;
			changeDepth(4);
			break;

		case KEY_F10:
			mDebug = !mDebug;
			break;

		case KEY_F2:
			save(constants::SAVE_GAME_PATH + "test.xml");
			break;

		case KEY_F3:
			load(constants::SAVE_GAME_PATH + "test.xml");
			break;
		case KEY_ESCAPE:
			clearMode();
			resetUi();
			break;

		default:
			break;
	}

	if(viewUpdated)
		mTileMap->mapViewLocation(pt.x(), pt.y());
}


/**
* Mouse Down event handler.
*/
void GameState::onMouseDown(MouseButton button, int x, int y)
{
	if (mDiggerDirection.visible() && isPointInRect(mMousePosition, mDiggerDirection.rect()))
		return;

	if (mWindowStack.pointInWindow(mMousePosition) && button == BUTTON_LEFT)
	{
		mWindowStack.updateStack(mMousePosition);
		return;
	}

	if (button == BUTTON_RIGHT)
	{
		Tile* _t = mTileMap->getTile(mTileMap->tileHighlight().x() + mTileMap->mapViewLocation().x(), mTileMap->tileHighlight().y() + mTileMap->mapViewLocation().y());

		if (mInsertMode != INSERT_NONE)
		{
			clearMode();
			return;
		}

		if (!mTileMap->tileHighlightVisible())
			return;

		if (!_t)
		{
			return;
		}
		else if (_t->empty() && isPointInRect(mMousePosition, mTileMap->boundingBox()))
		{
			clearSelections();
			mTileInspector.tile(_t);
			mTileInspector.show();
			mWindowStack.bringToFront(&mTileInspector);
		}
		else if (_t->thingIsStructure())
		{
			if (_t->structure()->isFactory() && (_t->structure()->operational() || _t->structure()->isIdle()))
			{
				mFactoryProduction.factory(static_cast<Factory*>(_t->structure()));
				mFactoryProduction.show();
				mWindowStack.bringToFront(&mFactoryProduction);
			}
			else
			{
				mStructureInspector.structure(_t->structure());
				mStructureInspector.show();
				mWindowStack.bringToFront(&mFactoryProduction);
			}
		}
	}

	if (button == BUTTON_LEFT)
	{
		mLeftButtonDown = true;

		mWindowStack.updateStack(mMousePosition);

		// Ugly
		if (isPointInRect(mMousePosition, MENU_ICON))
		{
			mReturnState = new PlanetSelectState();
			Utility<Renderer>::get().fadeOut(constants::FADE_SPEED);
			return;
		}

		// MiniMap Check
		if (isPointInRect(mMousePosition, mMiniMapBoundingBox) && !mWindowStack.pointInWindow(mMousePosition))
		{
			setMinimapView();
		}
		// Click was within the bounds of the TileMap.
		else if (isPointInRect(mMousePosition, mTileMap->boundingBox()))
		{
			if (mInsertMode == INSERT_STRUCTURE)
			{
				placeStructure();
			}
			else if (mInsertMode == INSERT_ROBOT)
			{
				placeRobot();
			}
			else if (mInsertMode == INSERT_TUBE)
			{
				placeTubes();
			}
		}
	}
}


/**
* Mouse Up event handler.
*/
void GameState::onMouseUp(MouseButton button, int x, int y)
{
	if (button == BUTTON_LEFT)
	{
		mLeftButtonDown = false;
	}
}


/**
* Mouse motion event handler.
*/
void GameState::onMouseMove(int x, int y, int rX, int rY)
{
	mMousePosition(x, y);

	if (mLeftButtonDown)
	{
		if (isPointInRect(mMousePosition, mMiniMapBoundingBox))
		{
			setMinimapView();
		}
	}

	mTileMapMouseHover(mTileMap->tileMouseHoverX(), mTileMap->tileMouseHoverY());
}


void GameState::onMouseWheel(int x, int y)
{
	if (mInsertMode != INSERT_TUBE)
		return;

	if (y > 0)
		mConnections.decrementSelection();
	else
		mConnections.incrementSelection();
}


bool GameState::changeDepth(int _d)
{
	int mPrevious = mTileMap->currentDepth();
	mTileMap->currentDepth(_d);

	if (mTileMap->currentDepth() == mPrevious)
		return false;

	clearMode();
	populateStructureMenu();
	updateCurrentLevelString(mTileMap->currentDepth());
	return true;
}


void GameState::clearMode()
{
	mInsertMode = INSERT_NONE;
	mCurrentPointer = POINTER_NORMAL;

	mCurrentStructure = SID_NONE;
	mCurrentRobot = ROBOT_NONE;

	clearSelections();
}


void GameState::insertTube(ConnectorDir _dir, int _depth, Tile* _t)
{
	if (_dir == CONNECTOR_INTERSECTION)
	{
		mStructureManager.addStructure(new Tube(CONNECTOR_INTERSECTION, _depth != 0), _t);
	}
	else if (_dir == CONNECTOR_RIGHT)
	{
		mStructureManager.addStructure(new Tube(CONNECTOR_RIGHT, _depth != 0), _t);
	}
	else if (_dir == CONNECTOR_LEFT)
	{
		mStructureManager.addStructure(new Tube(CONNECTOR_LEFT, _depth != 0), _t);
	}
	else
	{
		throw Exception(0, "Structure Not a Tube", "GameState::placeTube() called but Current Structure is not a tube!");
	}
}

void GameState::placeTubes()
{
	int x = mTileMapMouseHover.x();
	int y = mTileMapMouseHover.y();

	Tile* tile = mTileMap->getTile(x, y, mTileMap->currentDepth());
	if(!tile)
		return;

	// Check the basics.
	if (tile->thing() || tile->mine() || !tile->bulldozed() || !tile->excavated())
		return;
	
	ConnectorDir cd = static_cast<ConnectorDir>(mConnections.selectionIndex() + 1);
	if (validTubeConnection(x, y, cd))
	{
		insertTube(cd, mTileMap->currentDepth(), mTileMap->getTile(x, y));

		// FIXME: Naive approach -- will be slow with larger colonies.
		mStructureManager.disconnectAll();
		checkConnectedness();
	}
	else
		mAiVoiceNotifier.notify(AiVoiceNotifier::INVALID_TUBE_PLACEMENT);
}


/**
 * Checks to see if a tile is a valid tile to place a tube onto.
 */
bool GameState::validTubeConnection(int x, int y, ConnectorDir _cd)
{
	return	checkTubeConnection(mTileMap->getTile(x + 1, y, mTileMap->currentDepth()), DIR_EAST, _cd) ||
			checkTubeConnection(mTileMap->getTile(x - 1, y, mTileMap->currentDepth()), DIR_WEST, _cd) ||
			checkTubeConnection(mTileMap->getTile(x, y + 1, mTileMap->currentDepth()), DIR_SOUTH, _cd) ||
			checkTubeConnection(mTileMap->getTile(x, y - 1, mTileMap->currentDepth()), DIR_NORTH, _cd);
}


/**
 * Checks a tile to see if a valid Tube connection is available for Structure placement.
 */
bool GameState::validStructurePlacement(int x, int y)
{
	return	checkStructurePlacement(mTileMap->getTile(x, y - 1), DIR_NORTH) ||
			checkStructurePlacement(mTileMap->getTile(x + 1, y), DIR_EAST) ||
			checkStructurePlacement(mTileMap->getTile(x, y + 1), DIR_SOUTH) ||
			checkStructurePlacement(mTileMap->getTile(x - 1, y), DIR_WEST);
}


void GameState::placeRobot()
{
	Tile* tile = mTileMap->getTile(mTileMapMouseHover.x(), mTileMapMouseHover.y(), mTileMap->currentDepth());
	if(!tile)
		return;

	// Robodozer has been selected.
	if(mCurrentRobot == ROBOT_DOZER)
	{
		if(tile->mine() || !tile->excavated() || tile->thing() && !tile->thingIsStructure())
			return;
		else if (tile->thingIsStructure())
		{
			Structure* _s = tile->structure();
			if (_s->name() == constants::COMMAND_CENTER)
			{
				mAiVoiceNotifier.notify(AiVoiceNotifier::CC_NO_BULLDOZE);
				cout << "Can't bulldoze a Command Center!" << endl;
				return;
			}

			mPlayerResources.pushResources(StructureFactory::recyclingValue(StructureTranslator::translateFromString(_s->name())));

			tile->connected(false);
			mStructureManager.removeStructure(_s);
			tile->deleteThing();
			mStructureManager.disconnectAll();
			checkConnectedness();
		}
		else if (tile->index() == TERRAIN_DOZED)
			return;

		Robot* r = mRobotPool.getDozer();
		r->startTask(tile->index());
		insertRobotIntoTable(mRobotList, r, tile);
		tile->index(TERRAIN_DOZED);

		if(!mRobotPool.robotAvailable(ROBOT_DOZER))
		{
			mRobots.removeItem(constants::ROBODOZER);
			clearMode();
		}
	}
	// Robodigger has been selected.
	else if(mCurrentRobot == ROBOT_DIGGER)
	{
		// Keep digger within a safe margin of the map boundaries.
		if (mTileMapMouseHover.x() < 3 || mTileMapMouseHover.x() > mTileMap->width() - 4 || mTileMapMouseHover.y() < 3 || mTileMapMouseHover.y() > mTileMap->height() - 4)
		{
			mAiVoiceNotifier.notify(AiVoiceNotifier::INVALID_DIGGER_PLACEMENT);
			cout << "GameState::placeRobot(): Can't place digger within 3 tiles of the edge of a map." << endl;
			return;
		}

		if (!tile->excavated())
		{
			mAiVoiceNotifier.notify(AiVoiceNotifier::INVALID_DIGGER_PLACEMENT);
			return;
		}

		// Check for obstructions underneath the the digger location.
		if (tile->depth() != mTileMap->maxDepth() && !mTileMap->getTile(tile->x(), tile->y(), tile->depth() + 1)->empty())
		{
			cout << "Digger blocked underneath." << endl;
			mAiVoiceNotifier.notify(AiVoiceNotifier::INVALID_DIGGER_PLACEMENT);
			return;
		}

		// Die if tile is occupied or not excavated.
		if (!tile->empty())
		{
			
			if (tile->depth() > constants::DEPTH_SURFACE)
			{
				if (tile->thingIsStructure() && tile->structure()->connectorDirection() != CONNECTOR_VERTICAL) //air shaft
				{
					mAiVoiceNotifier.notify(AiVoiceNotifier::INVALID_DIGGER_PLACEMENT);
					return;
				}
				else if (tile->thingIsStructure() && tile->structure()->connectorDirection() == CONNECTOR_VERTICAL && tile->depth() == mTileMap->maxDepth())
				{
					mAiVoiceNotifier.notify(AiVoiceNotifier::MAX_DIGGING_DEPTH_REACHED);
					return;
				}
			}
			else
			{
				mAiVoiceNotifier.notify(AiVoiceNotifier::INVALID_DIGGER_PLACEMENT); // tile occupied
				return;
			}
		}
		
		if (!tile->thing() && mTileMap->currentDepth() > 0)
			mDiggerDirection.cardinalOnlyEnabled();
		else
			mDiggerDirection.downOnlyEnabled();

		mDiggerDirection.setParameters(tile);

		// NOTE:	Unlike the Dozer and Miner, Digger's aren't removed here but instead
		//			are removed after responses to the DiggerDirection dialog.

		// If we're placing on the top level we can only ever go down.
		if (mTileMap->currentDepth() == constants::DEPTH_SURFACE)
			mDiggerDirection.selectDown();
		else
			mDiggerDirection.visible(true);
	}
	// Robominer has been selected.
	else if(mCurrentRobot == ROBOT_MINER)
	{
		if (tile->thing() || !tile->mine() || !tile->excavated())
		{
			mAiVoiceNotifier.notify(AiVoiceNotifier::INVALID_MINER_PLACEMENT);
			return;
		}

		Robot* r = mRobotPool.getMiner();
		r->startTask(6);
		insertRobotIntoTable(mRobotList, r, tile);
		tile->index(TERRAIN_DOZED);

		if (!mRobotPool.robotAvailable(ROBOT_MINER))
		{
			mRobots.removeItem(constants::ROBOMINER);
			clearMode();
		}
	}
}


/** 
 * Checks the robot selection interface and if the robot is not available in it, adds
 * it back in and reeneables the robots button if it's not enabled.
 */
void GameState::checkRobotSelectionInterface(const std::string rType, int sheetIndex)
{
	if (!mRobots.itemExists(rType))
	{
		mRobots.addItem(rType, sheetIndex);
	}
}

/**
 * Called whenever a RoboDozer completes its task.
 */
void GameState::dozerTaskFinished(Robot* _r)
{
	checkRobotSelectionInterface(constants::ROBODOZER, constants::ROBODOZER_SHEET_ID);
}


/**
 * Called whenever a RoboDigger completes its task.
 */
void GameState::diggerTaskFinished(Robot* _r)
{
	if(mRobotList.find(_r) == mRobotList.end())
		throw Exception(0, "Renegade Robot", "GameState::diggerTaskFinished() called with a Robot not in the Robot List!");

	Tile* t = mRobotList[_r];

	if (t->depth() > mTileMap->maxDepth())
		throw Exception(0, "Bad Depth", "Digger defines a depth that exceeds the maximum digging depth!");

	// FIXME: Fugly cast.
	Direction dir = static_cast<Robodigger*>(_r)->direction();

	// 
	int originX = 0, originY = 0, depthAdjust = 0;

	if(dir == DIR_DOWN)
	{
		AirShaft* as1 = new AirShaft();
		if (t->depth() > 0) as1->ug();
		mStructureManager.addStructure(as1, t);

		AirShaft* as2 = new AirShaft();
		as2->ug();
		mStructureManager.addStructure(as2, mTileMap->getTile(t->x(), t->y(), t->depth() + 1));

		originX = t->x();
		originY = t->y();
		depthAdjust = 1;

		mTileMap->getTile(originX, originY, t->depth())->index(TERRAIN_DOZED);
		mTileMap->getTile(originX, originY, t->depth() + depthAdjust)->index(TERRAIN_DOZED);

		// FIXME: Naive approach; will be slow with large colonies.
		mStructureManager.disconnectAll();
		checkConnectedness();
	}
	else if(dir == DIR_NORTH)
	{
		originX = t->x();
		originY = t->y() - 1;
	}
	else if(dir == DIR_SOUTH)
	{
		originX = t->x();
		originY = t->y() + 1;
	}
	else if(dir == DIR_WEST)
	{
		originX = t->x() - 1;
		originY = t->y();
	}
	else if(dir == DIR_EAST)
	{
		originX = t->x() + 1;
		originY = t->y();
	}

	/**
	 * \todo	Add checks for obstructions and things that explode if
	 *			a digger gets in the way (or should diggers be smarter than
	 *			puncturing a fusion reactor containment vessel?)
	 */
	for(int y = originY - 1; y <= originY + 1; ++y)
	{
		for(int x = originX - 1; x <= originX + 1; ++x)
		{
			mTileMap->getTile(x, y, t->depth() + depthAdjust)->excavated(true);
		}
	}

	checkRobotSelectionInterface(constants::ROBODIGGER, constants::ROBODIGGER_SHEET_ID);
}


/**
 * Called whenever a RoboMiner completes its task.
 */
void GameState::minerTaskFinished(Robot* _r)
{
	if (mRobotList.find(_r) == mRobotList.end())
		throw Exception(0, "Renegade Robot", "GameState::minerTaskFinished() called with a Robot not in the Robot List!");

	Tile* t = mRobotList[_r];

	if (t->depth() == constants::DEPTH_SURFACE)
	{
		mStructureManager.addStructure(new MineFacility(t->mine()), t);
	}
	else
	{
		mStructureManager.addStructure(new MineShaft(), t);
	}

	Tile* t2 = mTileMap->getTile(t->x(), t->y(), t->depth() + 1);
	mStructureManager.addStructure(new MineShaft(), t2);

	t->index(0);
	t2->index(0);
	t2->excavated(true);

	checkRobotSelectionInterface(constants::ROBOMINER, constants::ROBOMINER_SHEET_ID);
}


/**
 * Called whenever a Factory's production is complete.
 */
void GameState::factoryProductionComplete(Factory::ProductionType _p)
{
	switch (_p)
	{
	case Factory::PRODUCTION_DIGGER:
		mRobotPool.addRobot(ROBOT_DIGGER)->taskComplete().Connect(this, &GameState::diggerTaskFinished);
		break;
	case Factory::PRODUCTION_DOZER:
		mRobotPool.addRobot(ROBOT_DOZER)->taskComplete().Connect(this, &GameState::dozerTaskFinished);
		break;
	case Factory::PRODUCTION_MINER:
		mRobotPool.addRobot(ROBOT_MINER)->taskComplete().Connect(this, &GameState::minerTaskFinished);
		break;
	default:
		cout << "Unknown production type." << endl;
		break;
	}
}

/**
 * Places a structure into the map.
 */
void GameState::placeStructure()
{
	if (mCurrentStructure == SID_NONE)
	{
		#ifdef _DEBUG
		throw Exception(0, "Invalid call to placeStructure()", "GameState::placeStructure() called but mCurrentStructure == STRUCTURE_NONE");
		#endif

		// When not in Debug just silently swallow this.
		return;
	}

	// Mouse is outside of the boundaries of the map so ignore this call.
	Tile* t = mTileMap->getTile(mTileMapMouseHover.x(), mTileMapMouseHover.y(), mTileMap->currentDepth());
	if(!t)
		return;

	if(t->mine() || t->thing() || !t->bulldozed() && mCurrentStructure != SID_SEED_LANDER)
	{
		mAiVoiceNotifier.notify(AiVoiceNotifier::INVALID_STRUCTURE_PLACEMENT);
		cout << "GameState::placeStructure(): Tile is unsuitable to place a structure." << endl;
		return;
	}

	// Seed lander is a special case and only one can ever be placed by the player ever.
	if(mCurrentStructure == SID_SEED_LANDER)
	{
		insertSeedLander(mTileMapMouseHover.x(), mTileMapMouseHover.y());
	}
	else
	{
		if (!validStructurePlacement(mTileMapMouseHover.x(), mTileMapMouseHover.y()))
		{
			mAiVoiceNotifier.notify(AiVoiceNotifier::INVALID_STRUCTURE_PLACEMENT);
			cout << "GameState::placeStructure(): Invalid structure placement." << endl;
			return;
		}

		// Check build cost
		ResourcePool rp = StructureFactory::costToBuild(mCurrentStructure);
		if (mPlayerResources.commonMetals() < rp.commonMetals() || mPlayerResources.commonMinerals() < rp.commonMinerals() ||
			mPlayerResources.rareMetals() < rp.rareMetals() || mPlayerResources.rareMinerals() < rp.rareMinerals())
		{
			mAiVoiceNotifier.notify(AiVoiceNotifier::INSUFFICIENT_RESOURCES);
			cout << "GameState::placeStructure(): Insufficient resources to build structure." << endl;
			return;
		}

		Structure* _s = StructureFactory::get(mCurrentStructure);
		if (_s)
			mStructureManager.addStructure(_s, t);

		// FIXME: Ugly
		if (_s->isFactory())
		{
			static_cast<Factory*>(_s)->productionComplete().Connect(this, &GameState::factoryProductionComplete);
			static_cast<Factory*>(_s)->resourcePool(&mPlayerResources);
		}

		mPlayerResources -= rp;
	}
}


void GameState::setMinimapView()
{
	int x = clamp(mMousePosition.x() - mMiniMapBoundingBox.x() - mTileMap->edgeLength() / 2, 0, mTileMap->width() - mTileMap->edgeLength());
	int y = clamp(mMousePosition.y() - mMiniMapBoundingBox.y() - mTileMap->edgeLength() / 2, 0, mTileMap->height() - mTileMap->edgeLength());

	mTileMap->mapViewLocation(x, y);
}





/**
 * Checks that the clicked tile is a suitable spot for the SEED Lander and
 * then inserts it into the the TileMap.
 */
void GameState::insertSeedLander(int x, int y)
{
	// Has to be built away from the edges of the map
	if (x > 3 && x < mTileMap->width() - 4 && y > 3 && y < mTileMap->height() - 4)
	{
		// check for obstructions
		if (!landingSiteSuitable(x, y))
		{
			mAiVoiceNotifier.notify(AiVoiceNotifier::UNSUITABLE_LANDING_SITE);
			cout << "Unable to place SEED Lander. Tiles obstructed." << endl;
			return;
		}

		SeedLander* s = new SeedLander(x, y);
		s->deployCallback().Connect(this, &GameState::deploySeedLander);
		mStructureManager.addStructure(s, mTileMap->getTile(x, y)); // Can only ever be placed on depth level 0

		clearMode();
		resetUi();

		mStructures.dropAllItems();
		mBtnTurns.enabled(true);
	}
	else
	{
		mAiVoiceNotifier.notify(AiVoiceNotifier::UNSUITABLE_LANDING_SITE);
	}
}


/**
 * Check landing site for obstructions such as mining beacons, things
 * and impassable terrain.
 */
bool GameState::landingSiteSuitable(int x, int y)
{
	for(int offY = y - 1; offY <= y + 1; ++offY)
		for(int offX = x - 1; offX <= x + 1; ++offX)
			if (mTileMap->getTile(offX, offY)->index() > TERRAIN_DIFFICULT || mTileMap->getTile(offX, offY)->mine() || mTileMap->getTile(offX, offY)->thing())
				return false;

	return true;
}


/**
 * Sets up the initial colony deployment.
 */
void GameState::deploySeedLander(int x, int y)
{
	mTileMap->getTile(x, y)->index(TERRAIN_DOZED);
	
	// TOP ROW
	mStructureManager.addStructure(new SeedPower(), mTileMap->getTile(x - 1, y - 1));
	mTileMap->getTile(x - 1, y - 1)->index(TERRAIN_DOZED);

	mStructureManager.addStructure(new Tube(CONNECTOR_INTERSECTION, false), mTileMap->getTile(x, y - 1));
	mTileMap->getTile(x, y - 1)->index(TERRAIN_DOZED);

	CommandCenter* cc = new CommandCenter();
	cc->sprite().skip(3);
	mStructureManager.addStructure(cc, mTileMap->getTile(x + 1, y - 1));
	mTileMap->getTile(x + 1, y - 1)->index(TERRAIN_DOZED);
	mCCLocation(x + 1, y - 1);


	// MIDDLE ROW
	mTileMap->getTile(x - 1, y)->index(TERRAIN_DOZED);
	mStructureManager.addStructure(new Tube(CONNECTOR_INTERSECTION, false), mTileMap->getTile(x - 1, y));

	mTileMap->getTile(x + 1, y)->index(TERRAIN_DOZED);
	mStructureManager.addStructure(new Tube(CONNECTOR_INTERSECTION, false), mTileMap->getTile(x + 1, y));


	// BOTTOM ROW
	SeedFactory* sf = new SeedFactory();
	sf->resourcePool(&mPlayerResources);
	sf->productionComplete().Connect(this, &GameState::factoryProductionComplete);
	sf->sprite().skip(7);
	mStructureManager.addStructure(sf, mTileMap->getTile(x - 1, y + 1));
	mTileMap->getTile(x - 1, y + 1)->index(TERRAIN_DOZED);

	mTileMap->getTile(x, y + 1)->index(TERRAIN_DOZED);
	mStructureManager.addStructure(new Tube(CONNECTOR_INTERSECTION, false), mTileMap->getTile(x, y + 1));

	SeedSmelter* ss = new SeedSmelter();
	ss->sprite().skip(10);
	mStructureManager.addStructure(ss, mTileMap->getTile(x + 1, y + 1));
	mTileMap->getTile(x + 1, y + 1)->index(TERRAIN_DOZED);

	// Robots only become available after the SEED Factor is deployed.
	mRobots.addItem(constants::ROBODOZER, constants::ROBODOZER_SHEET_ID);
	mRobots.addItem(constants::ROBODIGGER, constants::ROBODIGGER_SHEET_ID);
	mRobots.addItem(constants::ROBOMINER, constants::ROBOMINER_SHEET_ID);

	mRobotPool.addRobot(ROBOT_DOZER)->taskComplete().Connect(this, &GameState::dozerTaskFinished);
	mRobotPool.addRobot(ROBOT_DIGGER)->taskComplete().Connect(this, &GameState::diggerTaskFinished);
	mRobotPool.addRobot(ROBOT_MINER)->taskComplete().Connect(this, &GameState::minerTaskFinished);

	// FIXME: Magic numbers
	mPlayerResources.commonMetals(50);
	mPlayerResources.commonMinerals(50);
	mPlayerResources.rareMetals(30);
	mPlayerResources.rareMinerals(30);

	//mPopulationPool.addWorkers(30);
	//mPopulationPool.addScientists(20);
}



/**
 * Updates all robots.
 */
void GameState::updateRobots()
{
	auto robot_it = mRobotList.begin();
	while(robot_it != mRobotList.end())
	{
		robot_it->first->update();

		// Clear Idle robots from tiles.
		if(robot_it->first->idle())
		{
			// Make sure that we're the robot from a Tile and not something else
			if(robot_it->second->thing() == robot_it->first)
				robot_it->second->removeThing();
	
			robot_it = mRobotList.erase(robot_it);
		}
		else
			++robot_it;
	}
}


/**
 * Checks and sets the current structure mode.
 */
void GameState::setStructureID(StructureID type, InsertMode mode)
{

	if (type == SID_NONE)
	{
		clearMode();
		return;
	}

	mCurrentStructure = type;

	mInsertMode = mode;
	mCurrentPointer = POINTER_PLACE_TILE;
}


/**
 * Checks the connectedness of all tiles surrounding
 * the Command Center.
 */
void GameState::checkConnectedness()
{
	if (mCCLocation.x() == 0 && mCCLocation.y() == 0)
		return;

	// Assumes that the 'thing' at mCCLocation is in fact a Structure.
	Tile *t = mTileMap->getTile(mCCLocation.x(), mCCLocation.y(), 0);
	Structure *cc = t->structure();

	if (!cc)
		throw Exception(0, "Bad CC Location", "CC coordinates do not actually point to a Command Center.");

	if (cc->state() == Structure::UNDER_CONSTRUCTION)
		return;

	t->connected(true);

	// Start graph walking at the CC location.
	GraphWalker graphWalker(mCCLocation, 0, mTileMap);
}


void GameState::save(const std::string& _path)
{
	TiXmlDocument doc;

	TiXmlElement* root = new TiXmlElement(constants::SAVE_GAME_ROOT_NODE);
	root->SetAttribute("version", constants::SAVE_GAME_VERSION);
	doc.LinkEndChild(root);

	mTileMap->serialize(root);
	mStructureManager.serialize(root);
	writeRobots(root, mRobotPool, mRobotList);
	writeResources(root, mPlayerResources);

	TiXmlElement* turns = new TiXmlElement("turns");
	turns->SetAttribute("count", mTurnCount);
	root->LinkEndChild(turns);

	TiXmlElement* population = new TiXmlElement("population");
	turns->SetAttribute("morale", mCurrentMorale);
	root->LinkEndChild(population);

	TiXmlElement* ai = new TiXmlElement("ai");
	ai->SetAttribute("gender", mAiVoiceNotifier.gender());
	root->LinkEndChild(ai);


	// Write out the XML file.
	TiXmlPrinter printer;
	doc.Accept(&printer);

	Utility<Filesystem>::get().write(File(printer.Str(), _path));
}


void GameState::load(const std::string& _path)
{
	resetUi();

	File xmlFile = Utility<Filesystem>::get().open(_path);

	TiXmlDocument doc;
	TiXmlElement  *root = nullptr;

	// Load the XML document and handle any errors if occuring
	doc.Parse(xmlFile.raw_bytes());
	if (doc.Error())
	{
		cout << "Malformed savegame ('" << _path << "'). Error on Row " << doc.ErrorRow() << ", Column " << doc.ErrorCol() << ": " << doc.ErrorDesc() << endl;
		return;
	}

	root = doc.FirstChildElement(constants::SAVE_GAME_ROOT_NODE);
	if (root == nullptr)
	{
		cout << "Root element in '" << _path << "' is not '" << constants::SAVE_GAME_ROOT_NODE << "'." << endl;
		return;
	}

	std::string sg_version = root->Attribute("version");
	if(sg_version != constants::SAVE_GAME_VERSION)
	{
		cout << "Savegame version mismatch: '" << _path << "'. Expected " << constants::SAVE_GAME_VERSION << ", found " << sg_version << "." << endl;
		return;
	}
	
	// remove all robots currently deployed
	scrubRobotList();
	mPlayerResources.clear();
	mStructureManager.dropAllStructures();

	delete mTileMap;
	mTileMap = nullptr;

	//mTileMap->deserialize(root);
	TiXmlElement* map = root->FirstChildElement("properties");
	int depth = 0;
	map->Attribute("diggingdepth", &depth);
	string sitemap = map->Attribute("sitemap"), heightmap = map->Attribute("sitemap");
	mMapDisplay = Image(sitemap + MAP_DISPLAY_EXTENSION);
	mHeightMap = Image(sitemap + MAP_TERRAIN_EXTENSION);
	mTileMap = new TileMap(sitemap, map->Attribute("tset"), depth, false);
	mTileMap->deserialize(root);

	readStructures(root->FirstChildElement("structures"));
	readRobots(root->FirstChildElement("robots"));

	readResources(root->FirstChildElement("resources"), mPlayerResources);
	readTurns(root->FirstChildElement("turns"));
	readPopulation(root->FirstChildElement("population"));


	TiXmlElement* ai = root->FirstChildElement("ai");
	
	if(ai)
	{
		int gender = 0;
		ai->Attribute("gender", &gender);
		mAiVoiceNotifier.gender(static_cast<AiVoiceNotifier::AiGender>(gender));
	}

	mPlayerResources.capacity(totalStorage(mStructureManager.structureList(Structure::STRUCTURE_STORAGE)));
	
	// set level indicator string
	CURRENT_LEVEL_STRING = LEVEL_STRING_TABLE[mTileMap->currentDepth()];
}


void GameState::readRobots(TiXmlElement* _ti)
{
	mRobotPool.clear();
	mRobotList.clear();

	TiXmlNode* robot = _ti->FirstChild();
	for (robot; robot != nullptr; robot = robot->NextSibling())
	{
		int type = 0, age = 0, production_time = 0, x = 0, y = 0, depth = 0, direction = 0;
		robot->ToElement()->Attribute("type", &type);
		robot->ToElement()->Attribute("age", &age);
		robot->ToElement()->Attribute("production", &production_time);

		robot->ToElement()->Attribute("x", &x);
		robot->ToElement()->Attribute("y", &y);
		robot->ToElement()->Attribute("depth", &depth);
		robot->ToElement()->Attribute("direction", &direction);

		Robot* r = nullptr;
		RobotType rt = static_cast<RobotType>(type);
		switch (rt)
		{
		case ROBOT_DIGGER:
			r = mRobotPool.addRobot(ROBOT_DIGGER);
			r->taskComplete().Connect(this, &GameState::diggerTaskFinished);
			static_cast<Robodigger*>(r)->direction(static_cast<Direction>(direction));
			break;
		case ROBOT_DOZER:
			r = mRobotPool.addRobot(ROBOT_DOZER);
			r->taskComplete().Connect(this, &GameState::dozerTaskFinished);
			break;
		case ROBOT_MINER:
			r = mRobotPool.addRobot(ROBOT_MINER);
			r->taskComplete().Connect(this, &GameState::minerTaskFinished);
			break;
		default:
			cout << "Unknown robot type in savegame." << endl;
			break;
		}

		r->fuelCellAge(age);


		if (production_time > 0)
		{
			r->startTask(production_time);
			insertRobotIntoTable(mRobotList, r, mTileMap->getTile(x, y, depth));
			mRobotList[r]->index(0);
		}
		if (depth > 0)
			mRobotList[r]->excavated(true);
	}

	if (mRobotPool.robotAvailable(ROBOT_DIGGER))
		checkRobotSelectionInterface(constants::ROBODIGGER, constants::ROBODIGGER_SHEET_ID);
	if (mRobotPool.robotAvailable(ROBOT_DOZER))
		checkRobotSelectionInterface(constants::ROBODOZER, constants::ROBODOZER_SHEET_ID);
	if (mRobotPool.robotAvailable(ROBOT_MINER))
		checkRobotSelectionInterface(constants::ROBOMINER, constants::ROBOMINER_SHEET_ID);
}


void GameState::readStructures(TiXmlElement* _ti)
{
	for (TiXmlNode* structure = _ti->FirstChild(); structure != nullptr; structure = structure->NextSibling())
	{
		int x = 0, y = 0, depth = 0, id = 0, age = 0, state = 0, direction = 0;
		structure->ToElement()->Attribute("x", &x);
		structure->ToElement()->Attribute("y", &y);
		structure->ToElement()->Attribute("depth", &depth);
		structure->ToElement()->Attribute("id", &id);
		structure->ToElement()->Attribute("age", &age);
		structure->ToElement()->Attribute("state", &state);
		structure->ToElement()->Attribute("direction", &direction);

		Tile* t = mTileMap->getTile(x, y, depth);
		t->index(0);
		t->excavated(true);


		Structure* st = nullptr;
		string type = structure->ToElement()->Attribute("type");
		// case for tubes
		if (type == constants::TUBE)
		{
			ConnectorDir cd = static_cast<ConnectorDir>(direction);
			insertTube(cd, depth, mTileMap->getTile(x, y, depth));
			continue; // FIXME: ugly
		}

		StructureID type_id = StructureTranslator::translateFromString(type);
		st = StructureFactory::get(StructureTranslator::translateFromString(type));
				
		if (type_id == SID_COMMAND_CENTER)
			mCCLocation(x, y);

		if (type_id == SID_MINE_FACILITY)
		{
			Mine* m = mTileMap->getTile(x, y, 0)->mine();
			if (m == nullptr)
				throw Exception(0, "No Mine", "Mine Facility is located on a Tile with no Mine.");

			static_cast<MineFacility*>(st)->mine(m);
		}

		if (type_id == SID_AIR_SHAFT && depth > 0)
			static_cast<AirShaft*>(st)->ug();

		st->age(age);
		st->id(id);
		st->forced_state_change(static_cast<Structure::StructureState>(state));
		st->connectorDirection(static_cast<ConnectorDir>(direction));

		st->production().deserialize(structure->FirstChildElement("production"));
		st->storage().deserialize(structure->FirstChildElement("storage"));

		if (st->isFactory())
		{
			int production_completed = 0, production_type = 0;
			structure->ToElement()->Attribute("production_completed", &production_completed);
			structure->ToElement()->Attribute("production_type", &production_type);

			Factory* f = static_cast<Factory*>(st);
			f->productionType(static_cast<Factory::ProductionType>(production_type));
			f->productionTurnsCompleted(production_completed);
			f->resourcePool(&mPlayerResources);
			f->productionComplete().Connect(this, &GameState::factoryProductionComplete);
		}

		mStructureManager.addStructure(st, t);
	}

	checkConnectedness();
	mStructureManager.updateEnergyProduction(mPlayerResources);
}



void GameState::readTurns(TiXmlElement* _ti)
{
	if (_ti)
	{
		_ti->Attribute("count", &mTurnCount);

		if (mTurnCount > 0)
		{
			mBtnTurns.enabled(true);

			populateStructureMenu();
		}
	}
}


/**
 * Reads the population tag.
 */
void GameState::readPopulation(TiXmlElement* _ti)
{
	if (_ti)
	{
		_ti->Attribute("morale", &mCurrentMorale);

	}
}


/**
 * Removes deployed robots from the TileMap to
 * prevent dangling pointers. Yay for raw memory!
 */
void GameState::scrubRobotList()
{
	for (auto it = mRobotList.begin(); it != mRobotList.end(); ++it)
		it->second->removeThing();
}

/**
 * Update the value of the current level string
 */
void GameState::updateCurrentLevelString(int currentDepth)
{
	CURRENT_LEVEL_STRING = LEVEL_STRING_TABLE[currentDepth];
}
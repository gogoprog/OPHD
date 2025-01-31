// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com

#include <cmath>

#include "Tile.h"


/**
 * C'tor
 */
Tile::Tile()
{}


/**
 * D'tor
 */
Tile::~Tile()
{
	delete mMine;
	delete mThing;
}


/**
 * Convenience function that inits the Tile with all pertinent information.
 */
void Tile::init(int _x, int _y, int _depth, int _index)
{
	x(_x);
	y(_y);
	depth(_depth);
	index(_index);
}


/**
 * Adds a new Thing to the tile.
 *
 * \param	thing		Pointer to a Thing.
 * \param	overwrite	Overwrite any existing Thing's that may already be in the Tile.
 */
void Tile::pushThing(Thing* thing, bool overwrite)
{
	if (mThing)
	{
		if (overwrite)
		{
			deleteThing();
		}
		else
		{
			// Clean up the thing passed into this function
			// as they're not references but newly created objects.
			//
			// fixme:	This is a hell of an assumption -- is it correct?
			delete thing;
			return;
		}
	}

	mThing = thing;
}


/**
 * Clears a Thing from the Tile.
 *
 * \note	Garbage collects the Thing. Deletes and frees memory.
 */
void Tile::deleteThing()
{
	delete mThing;
	removeThing();
}


/**
 * Removes a Thing from the Tile.
 *
 * \note	Does NOT delete or free memory. Simply clears the pointer, not the memory.
 */
void Tile::removeThing()
{
	mThing = nullptr;
	thingIsStructure(false);	// Cover all bases.
}


/**
 * 
 */
void Tile::pushMine(Mine* _mine)
{
	delete mMine;
	mMine = _mine;
}


/**
 * 
 */
Structure* Tile::structure()
{
	if (mThingIsStructure) { return static_cast<Structure*>(thing()); }

	return nullptr;
}


/**
 * 
 */
Robot* Tile::robot()
{
	// Assumption: Things in a tile can only be a Robot or a Structure. If the thing is not a
	// structure, it can only be a robot.
	if (!empty() && structure() == nullptr) { return static_cast<Robot*>(thing()); }

	return nullptr;
}


/**
 * 
 */
float Tile::distanceTo(Tile* t)
{
	int x = t->x() - Tile::x();
	int y = t->y() - Tile::y();
	return sqrt((x * x) + (y * y));
}

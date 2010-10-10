/* ScummVM - Graphic Adventure Engine
*
* ScummVM is the legal property of its developers, whose names
* are too numerous to list here. Please refer to the COPYRIGHT
* file distributed with this source distribution.
*
* This program is free software; you can redistribute it and/or
* modify it under the terms of the GNU General Public License
* as published by the Free Software Foundation; either version 2
* of the License, or (at your option) any later version.

* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.

* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
*
* $URL$
* $Id$
*
*/

#include "toon/character.h"
#include "toon/drew.h"
#include "toon/path.h"

namespace Toon {

Character::Character(ToonEngine *vm) : _vm(vm) {
	_animationInstance = 0;
	_shadowAnimationInstance = 0;
	_shadowAnim = 0;
	_x = 0;
	_y = 0;
	_z = 0;
	_finalX = 0;
	_finalY = 0;
	_specialAnim = 0;
	_sceneAnimationId = -1;
	_idleAnim = 0;
	_walkAnim = 0;
	_talkAnim = 0;
	_facing = 0;
	_flags = 0;
	_animFlags = 0;
	_id = 0;
	_scale = 1024;
	_blockingWalk = false;
	_animScriptId = -1;
	_animSpecialId = -1;
	_animSpecialDefaultId = 0;
	_currentPathNodeCount = 0;
	_currentPathNode = 0;
	_visible = true;
	_speed = 150;   // 150 = nominal drew speed
	_lastWalkTime = 0;
	_numPixelToWalk = 0;
	_nextIdleTime = _vm->getSystem()->getMillis() + (_vm->randRange(0, 600) + 300) * _vm->getTickLength();
}

Character::~Character(void) {
}

void Character::init() {

}

void Character::setFacing(int32 facing) {
	debugC(4, kDebugCharacter, "setFacing(%d)", facing);
	_facing = facing;
}

void Character::setPosition(int32 x, int32 y) {
	debugC(5, kDebugCharacter, "setPosition(%d, %d)", x, y);

	_x = x;
	_y = y;
	if (_animationInstance)
		_animationInstance->setPosition(_x, _y, _z);
	return;
}

bool Character::walkTo(int32 newPosX, int32 newPosY) {
	debugC(1, kDebugCharacter, "walkTo(%d, %d)", newPosX, newPosY);

	if (!_visible)
		return true;

	if (_x == newPosX && _y == newPosY)
		return true;

	_vm->getPathFinding()->resetBlockingRects();

	if (_id == 1) {
		int32 sizeX = MAX(5, 40 * _vm->getDrew()->getScale() / 1024);
		int32 sizeY = MAX(2, 20 * _vm->getDrew()->getScale() / 1024);
		_vm->getPathFinding()->addBlockingEllipse(_vm->getDrew()->getFinalX(), _vm->getDrew()->getFinalY(), sizeX, sizeY);
	}

	_vm->getPathFinding()->findClosestWalkingPoint(newPosX, newPosY, &_finalX, &_finalY, _x, _y);
	if (_x == _finalX && _y == _finalY)
		return true;


	if (_vm->getPathFinding()->findPath(_x, _y, _finalX, _finalY)) {

		int32 localFinalX = _finalX;
		int32 localFinalY = _finalY;

		for (int32 a = 0; a < _vm->getPathFinding()->getPathNodeCount(); a++) {
			_currentPathX[a] = _vm->getPathFinding()->getPathNodeX(a);
			_currentPathY[a] = _vm->getPathFinding()->getPathNodeY(a);
		}
		_currentPathNodeCount = _vm->getPathFinding()->getPathNodeCount();
		_currentPathNode = 0;
		stopSpecialAnim();
		_flags |= 0x1;
		_lastWalkTime = _vm->getSystem()->getMillis();

		_numPixelToWalk = 0;

		if (_blockingWalk) {
			while ((_x != newPosX || _y != newPosY) && _currentPathNode < _currentPathNodeCount && !_vm->shouldQuit()) {
				if (_currentPathNode < _currentPathNodeCount - 10) {
					int32 delta = MIN(10, _currentPathNodeCount - _currentPathNode);
					int32 dx = _currentPathX[_currentPathNode+delta] - _x;
					int32 dy = _currentPathY[_currentPathNode+delta] - _y;
					setFacing(getFacingFromDirection(dx, dy));
					playWalkAnim(0, 0);
				}

				// in 1/1000 pixels
				_numPixelToWalk += _speed * (_vm->getSystem()->getMillis() - _lastWalkTime) * _scale / 1024;
				_lastWalkTime =  _vm->getSystem()->getMillis();

				while (_numPixelToWalk >= 1000 && _currentPathNode < _currentPathNodeCount) {
					_x = _currentPathX[_currentPathNode];
					_y = _currentPathY[_currentPathNode];
					_currentPathNode += 1;
					_numPixelToWalk -= 1000;
				}
				setPosition(_x, _y);

				_vm->doFrame();
			}
			playStandingAnim();
			_flags &= ~0x1;
			_currentPathNode = 0;
			_currentPathNodeCount = 0;

			if (_x != localFinalX || _y != localFinalY) {
				return false;
			}
		}
	}

	//_vm->getPathFinding()->findClosestWalkingPoint(newPosX, newPosY, &_x, &_y, _x, _y);
	//setPosition(_x,_y);
	return true;
}

void Character::setFlag(int flag) {
	_flags = flag;
}

int32 Character::getFlag() {
	return _flags;
}

int32 Character::getX() {
	return _x;
}
int32 Character::getY() {
	return _y;
}

bool Character::getVisible() {
	return _visible;
}

void Character::setVisible(bool visible) {
	debugC(1, kDebugCharacter, "setVisible(%d)", (visible) ? 1 : 0);

	_visible = visible;
	if (_animationInstance)
		_animationInstance->setVisible(visible);

	if (_shadowAnimationInstance)
		_shadowAnimationInstance->setVisible(visible);

	return;
}

int32 Character::getFacing() {
	return _facing;
}

bool Character::loadWalkAnimation(Common::String animName) {
	debugC(1, kDebugCharacter, "loadWalkAnimation(%s)", animName.c_str());
	if (_walkAnim)
		delete _walkAnim;

	_walkAnim = new Animation(_vm);
	return _walkAnim->loadAnimation(animName);
}

bool Character::loadIdleAnimation(Common::String animName) {
	debugC(1, kDebugCharacter, "loadIdleAnimation(%s)", animName.c_str());
	if (_idleAnim)
		delete _idleAnim;

	_idleAnim = new Animation(_vm);
	return _idleAnim->loadAnimation(animName);
}

bool Character::loadTalkAnimation(Common::String animName) {
	debugC(1, kDebugCharacter, "loadTalkAnimation(%s)", animName.c_str());
	if (_talkAnim)
		delete _talkAnim;

	_talkAnim = new Animation(_vm);
	return _talkAnim->loadAnimation(animName);
}

bool Character::setupPalette() {
	return false; // only for Drew
}

void Character::playStandingAnim() {

}

void Character::stopSpecialAnim() {
	debugC(4, kDebugCharacter, "stopSpecialAnim()");
// Strangerke - Commented (not used)
#if 0
	if (_animSpecialId != _animSpecialDefaultId)
		delete anim
#endif
	if (_animScriptId != -1)
		_vm->getSceneAnimationScript(_animScriptId)->_frozen = false;

	if (_sceneAnimationId != -1)
		_animationInstance->setAnimation(_vm->getSceneAnimation(_sceneAnimationId)->_animation);

	bool needStandingAnim = (_animFlags & 0x40) != 0;

	_animSpecialId = -1;
	_time = 0;
	_animFlags = 0;
	_flags &= ~1;
	_flags &= ~4;
	
	if (needStandingAnim) {
		playStandingAnim();
	}
}

void Character::update(int32 timeIncrement) {
	debugC(5, kDebugCharacter, "update(%d)", timeIncrement);
	if ((_flags & 0x1) && _currentPathNodeCount > 0) {
		if (_currentPathNode < _currentPathNodeCount) {
			if (_currentPathNode < _currentPathNodeCount - 10) {
				int32 delta = MIN(10, _currentPathNodeCount - _currentPathNode);
				int32 dx = _currentPathX[_currentPathNode+delta] - _x;
				int32 dy = _currentPathY[_currentPathNode+delta] - _y;
				setFacing(getFacingFromDirection(dx, dy));
				playWalkAnim(0, 0);
			}

			// in 1/1000 pixels
			_numPixelToWalk += _speed * (_vm->getSystem()->getMillis() - _lastWalkTime) * _scale / 1024;
			_lastWalkTime =  _vm->getSystem()->getMillis();

			while (_numPixelToWalk > 1000 && _currentPathNode < _currentPathNodeCount) {
				_x = _currentPathX[_currentPathNode];
				_y = _currentPathY[_currentPathNode];
				_currentPathNode += 1;
				_numPixelToWalk -= 1000;
			}
			setPosition(_x, _y);
		} else {
			playStandingAnim();
			_flags &= ~0x1;
			_currentPathNodeCount = 0;
		}
	}

	updateIdle();

#if 0
	// handle special anims
	if ((_flags & 4) == 0)
		return;


	if (_animScriptId != -1) {
		_animationInstance = _vm->getSceneAnimation(this->)
#endif

	int32 animId = _animSpecialId;
	if (animId >= 1000)
		animId = 0;

	if (_animSpecialId < 0)
		return;

	int32 currentFrame = _animationInstance->getFrame();

	const SpecialCharacterAnimation *anim = getSpecialAnimation(_id, animId);

	if ((_animFlags & 0x10) == 0) {
		if (_animScriptId != -1 && currentFrame > 0 && !_vm->getSceneAnimationScript(_animScriptId)->_frozen) {
			if (_vm->getCurrentLineToSay() != _lineToSayId && (_animFlags & 8))
				stopSpecialAnim();
			return;
		}

		if (_id == 1 && (_animFlags & 4)) {
			if (_animFlags & 0x10)
				return;
		} else {
			if (_id == 1 && (_animFlags & 0x10) && _vm->getCurrentLineToSay() != -1) {
				return;
			}
			if ((_animFlags & 0x40) == 0 && _vm->getCurrentLineToSay() == -1) {
				stopSpecialAnim();
				return;
			}

// Strangerke - Commented (not used)
#if 0
			if (_animFlags & 8) {
				if (anim->_flags7 == 0xff && anim->_flags9 == 0xff) {
					// start voice
				}
			}
#endif

			if (_animScriptId != -1)
				_vm->getSceneAnimationScript(_animScriptId)->_frozen = true;

			// TODO setup backup //

			_animFlags |= 0x10;
			_animationInstance->setFrame(0);
			_time = _vm->getOldMilli() + 8 * _vm->getTickLength();
		}

	}

	if ((_animFlags & 3) == 2) {
		if (_vm->getCurrentLineToSay() != _lineToSayId || !_vm->getAudioManager()->voiceStillPlaying())  // || (_flags & 8)) && _vm->getAudioManager()->voiceStillPlaying())
			_animFlags |= 1;

// Strangerke - Commented (not used)
//	} else {
	}

	// label29 :
	if (_time > _vm->getOldMilli())
		return;

	int32 animFlag = anim->_unused;
	int32 nextFrame = currentFrame + 1;
	int32 nextTime = _time;
	int32 animDir = 1;
	if (!animFlag) {
		if (_animFlags & 1) {
			if (anim->_flags7 == 0xff) {
				if (currentFrame > anim->_flag1 / 2)
					animDir = 1;
				else
					animDir = -1;
			} else {
				if (currentFrame >= anim->_flags6) {
					if (currentFrame < anim->_flags7)
						currentFrame = anim->_flags7;
				}
				if (currentFrame > anim->_flags6)
					animDir = 1;
				else
					animDir = -1;
			}

			nextFrame = currentFrame + animDir;
			nextTime = _vm->getOldMilli() + 6 * _vm->getTickLength();
		} else {
			if (_animFlags & 0x20) {
				nextFrame = currentFrame - 1;
				if (nextFrame == anim->_flags6 - 1) {
					if (anim->_flags8 != 1 && (_vm->randRange(0, 1) == 1 || anim->_flags8 == 2)) {
						_animFlags &= ~0x20;
						nextFrame += 2;
						if (nextFrame > anim->_flags7)
							nextFrame = anim->_flags7;
					} else {
						nextFrame = anim->_flags7;
					}
				}
			} else {
				nextFrame = currentFrame + 1;
// Strangerke - Commented (not used)
#if 0
				if (!_vm->getAudioManager()->voiceStillPlaying()) {
					if (_animFlags & 8) {
						if ((anim->_flags9 == 0xff && nextFrame == anim->_flags6) ||
						    (anim->_flags9 != 0xff && nextFrame >= anim->_flags9)) {
							// start really talking
						}
					}
				}
#endif
				if (nextFrame == anim->_flags7 + 1 && (_animFlags & 0x40) == 0) {
					if (anim->_flags8 != 1 && (_vm->randRange(0, 1) || anim->_flags8 == 2)) {
						_animFlags |= 0x20;
						nextFrame -= 2;
						if (nextFrame < anim->_flags6)
							nextFrame = anim->_flags6;
					} else {
						nextFrame = anim->_flags6;
					}
				}
			}

			nextTime = _vm->getOldMilli() + 8 * _vm->getTickLength();
		}
		// goto label78
	}
	// skipped all this part.

	//label78
#if 0
	if (_id == 0)
		debugC(0, 0xfff, " drew animation flag %d / frame %d", _animFlags, nextFrame);

	if (_id == 1)
		debugC(0, 0xfff, " flux animation flag %d / frame %d", _animFlags, nextFrame);

	if (_id == 7)
		debugC(0, 0xfff, " footman animation flag %d / frame %d", _animFlags, nextFrame);
#endif

	_time = nextTime;
	if (nextFrame < 0 || nextFrame >= anim->_flag1) {
		if ((_animFlags & 2) == 0 || _vm->getCurrentLineToSay() != _lineToSayId) {
			stopSpecialAnim();
			return;
		}

		// lots skipped here

		_animFlags &= ~0x10;
		_animationInstance->forceFrame(0);
		return;

	}

	//if ((_flags & 8) == 0 || !_vm->getAudioManager()->voiceStillPlaying( ) || )
	_animationInstance->forceFrame(nextFrame);
}

// adapted from Kyra
int32 Character::getFacingFromDirection(int32 dx, int32 dy) {
	debugC(4, kDebugCharacter, "getFacingFromDirection(%d, %d)", dx, dy);

	static const int facingTable[] = {
		//1, 0, 1, 2,  3, 4, 3, 2,  7, 0, 7, 6,  5, 4, 5, 6
		5, 6, 5, 4,  3, 2, 3, 4,  7, 6, 7, 0,  1, 2, 1, 0
	};
	dx = -dx;

	int32 facingEntry = 0;
	int32 ydiff = dy;
	if (ydiff < 0) {
		++facingEntry;
		ydiff = -ydiff;
	}
	facingEntry <<= 1;

	int32 xdiff = dx;
	if (xdiff < 0) {
		++facingEntry;
		xdiff = -xdiff;
	}

	facingEntry <<= 1;

	if (xdiff >= ydiff) {
		int32 temp = ydiff;
		ydiff = xdiff;
		xdiff = temp;
	} else {
		facingEntry += 1;
	}

	facingEntry <<= 1;

	int32 temp = (ydiff + 1) >> 1;

	if (xdiff < temp)
		facingEntry += 1;

	return facingTable[facingEntry];
}

AnimationInstance *Character::getAnimationInstance() {
	return _animationInstance;
}

void Character::setAnimationInstance(AnimationInstance *instance) {
	_animationInstance = instance;
}

int32 Character::getScale() {
	return _scale;
}

void Character::playWalkAnim(int32 startFrame, int32 endFrame) {

}

void Character::setId(int32 id) {
	_id = id;
}

int32 Character::getId() {
	return _id;
}

void Character::save(Common::WriteStream *stream) {
	debugC(1, kDebugCharacter, "save(stream)");

	stream->writeSint32LE(_flags);
	stream->writeSint32LE(_x);
	stream->writeSint32LE(_y);
	stream->writeSint32LE(_z);
	stream->writeSint32LE(_finalX);
	stream->writeSint32LE(_finalY);
	stream->writeSint32LE(_scale);
	stream->writeSint32LE(_id);

	stream->writeSint32LE(_animScriptId);
	stream->writeSint32LE(_animFlags);
	stream->writeSint32LE(_animSpecialDefaultId);
	stream->writeSint32LE(_sceneAnimationId);
}

void Character::load(Common::ReadStream *stream) {
	debugC(1, kDebugCharacter, "read(stream)");

	_flags = stream->readSint32LE();
	_x = stream->readSint32LE();
	_y = stream->readSint32LE();
	_z = stream->readSint32LE();
	_finalX = stream->readSint32LE();
	_finalY = stream->readSint32LE();
	_scale = stream->readSint32LE();
	_id = stream->readSint32LE();

	_animScriptId = stream->readSint32LE();
	_animFlags = stream->readSint32LE();
	_animSpecialDefaultId = stream->readSint32LE();
	_sceneAnimationId = stream->readSint32LE();

	if (_sceneAnimationId > -1) {
		setAnimationInstance(_vm->getSceneAnimation(_sceneAnimationId)->_animInstance);
	}
}

void Character::setAnimScript(int32 animScriptId) {
	_animScriptId = animScriptId;
}

void Character::setSceneAnimationId(int32 sceneAnimationId) {
	_sceneAnimationId = sceneAnimationId;
}

int32 Character::getAnimScript() {
	return _animScriptId;
}

void Character::playTalkAnim() {

}

void Character::stopWalk() {
	debugC(1, kDebugCharacter, "stopWalk()");

	_finalX = _x;
	_finalY = _y;
	_flags &= ~0x1;
	_currentPathNode = 0;
	_currentPathNodeCount = 0;
}

const SpecialCharacterAnimation *Character::getSpecialAnimation(int32 characterId, int32 animationId) {
	debugC(6, kDebugCharacter, "getSpecialAnimation(%d, %d)", characterId, animationId);

	// very nice animation list hardcoded in the executable...
	static const SpecialCharacterAnimation anims[] = {
		{ "TLK547_?", 9, 0, 0, 0, 0, 0, 1, 5, 8, 1, 8, 0, 255 },
		{ "TLK555_?", 16, 0, 0, 0, 0, 6, 8, 10, 255, 6, 11, 2, 255 },
		{ "LST657_?", 14, 0, 0, 0, 0, 255, 255, 255, 255, 5, 11, 0, 255 },
		{ "TLK587_?", 18, 0, 0, 0, 0, 5, 7, 9, 11, 4, 13, 1, 255 },
		{ "LST659_?", 14, 0, 0, 0, 0, 255, 255, 255, 255, 6, 8, 0, 255 },
		{ "TLK595_?", 11, 0, 0, 0, 0, 3, 6, 255, 255, 1, 7, 0, 255 },
		{ "IDL165_?", 13, 0, 0, 0, 0, 255, 255, 255, 255, 6, 8, 0, 255 },
		{ "LST699_?", 10, 0, 0, 0, 0, 255, 255, 255, 255, 1, 9, 1, 255 },
		{ "LST713_?", 10, 0, 0, 0, 0, 255, 255, 255, 255, 4, 6, 0, 255 },
		{ "IDL169_?", 16, 0, 0, 0, 0, 255, 255, 255, 255, 5, 9, 2, 255 },
		{ "IDL173_?", 19, 0, 0, 0, 0, 255, 255, 255, 255, 4, 17, 1, 255 },
		{ "IDL187_?", 14, 0, 0, 0, 0, 255, 255, 255, 255, 4, 8, 0, 255 },
		{ "IDL185_?", 15, 0, 0, 0, 0, 255, 255, 255, 255, 6, 9, 1, 255 },
		{ "TLK635_?", 16, 0, 0, 0, 0, 5, 8, 10, 12, 4, 12, 0, 255 },
		{ "TLK637_?", 18, 0, 0, 0, 0, 5, 7, 9, 12, 4, 13, 0, 255 },
		{ "TLK551_?", 20, 0, 0, 0, 0, 5, 9, 11, 15, 4, 16, 0, 255 },
		{ "TLK553_?", 20, 0, 0, 0, 0, 7, 9, 11, 13, 6, 15, 0, 255 },
		{ "TLK619_?", 18, 0, 0, 0, 0, 5, 8, 11, 13, 5, 15, 0, 255 },
		{ "TLK601_?", 12, 0, 0, 0, 0, 2, 5, 6, 10, 2, 10, 1, 255 },
		{ "TLK559_?", 18, 0, 0, 0, 0, 4, 6, 10, 12, 4, 13, 0, 255 },
		{ "TLK557_?", 16, 0, 0, 0, 0, 6, 8, 10, 255, 6, 11, 0, 255 },
		{ "TLK561_?", 17, 0, 0, 0, 0, 6, 8, 10, 12, 5, 12, 0, 255 },
		{ "TLK623_?", 19, 0, 0, 0, 0, 6, 8, 10, 13, 6, 14, 0, 255 },
		{ "TLK591_?", 20, 0, 0, 0, 0, 10, 14, 255, 255, 7, 15, 0, 255 },
		{ "TLK567_?", 19, 0, 0, 0, 0, 6, 9, 11, 14, 5, 15, 0, 255 },
		{ "TLK629_?", 18, 0, 0, 0, 0, 6, 8, 10, 11, 6, 12, 0, 255 },
		{ "TLK627_?", 19, 0, 0, 0, 0, 7, 10, 12, 14, 4, 14, 0, 255 },
		{ "TLK631_?", 19, 0, 0, 0, 0, 8, 10, 255, 255, 8, 12, 0, 255 },
		{ "TLK565_?", 17, 0, 0, 0, 0, 4, 7, 9, 11, 3, 12, 0, 255 },
		{ "TLK603_?", 16, 0, 0, 0, 0, 5, 255, 255, 255, 3, 9, 0, 255 },
		{ "TLK573_?", 20, 0, 0, 0, 0, 6, 7, 10, 255, 6, 16, 2, 255 },
		{ "TLK615_?", 17, 0, 0, 0, 0, 6, 8, 10, 12, 5, 12, 0, 255 },
		{ "TLK609_?", 18, 0, 0, 0, 0, 6, 8, 10, 12, 5, 13, 0, 255 },
		{ "TLK611_?", 18, 0, 0, 0, 0, 8, 10, 12, 255, 7, 13, 0, 255 },
		{ "TLK607_?", 16, 0, 0, 0, 0, 4, 7, 9, 11, 4, 12, 0, 255 },
		{ "TLK581_?", 15, 0, 0, 0, 0, 7, 9, 11, 255, 6, 11, 0, 255 },
		{ "SHD107_?", 46, 0, 0, 0, 0, 255, 255, 255, 255, 255, 255, 0, 255 },
		{ "IHL106_?", 23, 0, 0, 0, 0, 255, 255, 255, 255, 255, 255, 0, 7 },
		{ "GLV106_?", 23, 0, 0, 0, 0, 255, 255, 255, 255, 255, 255, 0, 7 },
		{ "FXTKA_?", 11, 0, 0, 0, 0, 7, 255, 255, 255, 2, 9, 0, 255 },
		{ "FXTKF_?", 12, 0, 0, 0, 0, 6, 8, 255, 255, 5, 9, 0, 255 },
		{ "FXTKG_?", 9, 0, 0, 0, 0, 5, 255, 255, 255, 4, 7, 0, 255 },
		{ "FXTKI_?", 12, 0, 0, 0, 0, 6, 255, 255, 255, 5, 9, 0, 255 },
		{ "FXTKL_?", 14, 0, 0, 0, 0, 4, 6, 255, 255, 3, 10, 0, 255 },
		{ "FXTKO_?", 10, 0, 0, 0, 0, 4, 255, 255, 255, 4, 7, 0, 255 },
		{ "FXTKP_?", 9, 0, 0, 0, 0, 4, 6, 255, 255, 3, 7, 0, 255 },
		{ "FXTKQ_?", 10, 0, 0, 0, 0, 4, 6, 255, 255, 3, 7, 0, 255 },
		{ "FXLSA_?", 11, 0, 0, 0, 0, 255, 255, 255, 255, 4, 6, 0, 255 },
		{ "FXLSB_?", 9, 0, 0, 0, 0, 255, 255, 255, 255, 4, 5, 0, 255 },
		{ "FXLSK_?", 8, 0, 0, 0, 0, 255, 255, 255, 255, 5, 6, 0, 255 },
		{ "FXLSM_?", 7, 0, 0, 0, 0, 255, 255, 255, 255, 4, 4, 0, 255 },
		{ "FXLSP_?", 7, 0, 0, 0, 0, 255, 255, 255, 255, 3, 3, 0, 255 },
		{ "FXLSQ_?", 6, 0, 0, 0, 0, 255, 255, 255, 255, 3, 3, 0, 255 },
		{ "FXIDE_?", 10, 0, 0, 0, 0, 255, 255, 255, 255, 5, 7, 0, 255 },
		{ "FXIDI_?", 7, 0, 0, 0, 0, 255, 255, 255, 255, 1, 6, 1, 255 },
		{ "FXRCT1_?", 12, 0, 0, 0, 0, 255, 255, 255, 255, 255, 255, 0, 255 },
		{ "FXTKB_?", 11, 0, 0, 0, 0, 7, 255, 255, 255, 5, 9, 0, 255 },
		{ "FXTKC_?", 14, 0, 0, 0, 0, 2, 5, 8, 10, 1, 12, 2, 255 },
		{ "FXTKD_?", 14, 0, 0, 0, 0, 5, 7, 9, 255, 4, 11, 0, 255 },
		{ "FXTKE_?", 14, 0, 0, 0, 0, 2, 255, 255, 255, 1, 12, 1, 255 },
		{ "FXTKH_?", 11, 0, 0, 0, 0, 6, 8, 255, 255, 4, 9, 0, 255 },
		{ "FXTKJ_?", 8, 0, 0, 0, 0, 7, 255, 255, 255, 4, 7, 0, 255 },
		{ "FXTKK_?", 13, 0, 0, 0, 0, 6, 8, 255, 255, 5, 9, 0, 255 },
		{ "FXTKM_?", 11, 0, 0, 0, 0, 6, 255, 255, 255, 4, 7, 0, 255 },
		{ "FXTKN_?", 9, 0, 0, 0, 0, 5, 7, 255, 255, 4, 7, 0, 255 },
		{ "FXLSC_?", 9, 0, 0, 0, 0, 255, 255, 255, 255, 3, 6, 1, 255 },
		{ "FXLSD_?", 7, 0, 0, 0, 0, 255, 255, 255, 255, 4, 5, 0, 255 },
		{ "FXLSE_?", 9, 0, 0, 0, 0, 255, 255, 255, 255, 8, 8, 0, 255 },
		{ "FXLSG_?", 11, 0, 0, 0, 0, 255, 255, 255, 255, 6, 8, 2, 255 },
		{ "FXLSI_?", 8, 0, 0, 0, 0, 255, 255, 255, 255, 5, 6, 0, 255 },
		{ "FXLSJ_?", 5, 0, 0, 0, 0, 255, 255, 255, 255, 3, 4, 0, 255 },
		{ "FXLSO_?", 8, 0, 0, 0, 0, 255, 255, 255, 255, 4, 5, 0, 255 },
		{ "FXIDA_?", 15, 0, 0, 0, 0, 255, 255, 255, 255, 1, 12, 1, 255 },
		{ "FXIDB_?", 12, 0, 0, 0, 0, 255, 255, 255, 255, 4, 11, 1, 255 },
		{ "FXIDC_?", 11, 0, 0, 0, 0, 255, 255, 255, 255, 7, 7, 0, 255 },
		{ "FXIDD_?", 15, 0, 0, 0, 0, 255, 255, 255, 255, 6, 6, 0, 255 },
		{ "FXIDG_?", 6, 0, 0, 0, 0, 255, 255, 255, 255, 3, 4, 0, 255 },
		{ "FXVRA_?", 7, 0, 0, 0, 0, 255, 255, 255, 255, 2, 6, 2, 255 },
		{ "FXIDF_?", 15, 0, 0, 0, 0, 255, 255, 255, 255, 9, 11, 0, 255 },
		{ "FXEXA_?", 9, 0, 0, 0, 0, 255, 255, 255, 255, 5, 5, 0, 255 },
		{ "FXEXA_?", 9, 0, 0, 0, 0, 255, 255, 255, 255, 5, 5, 0, 255 },
		{ "FFNTK1", 8, 0, 0, 0, 0, 255, 255, 255, 255, 1, 7, 0, 255 },
		{ "FFTLK1", 10, 0, 0, 0, 0, 255, 255, 255, 255, 1, 9, 0, 1 },
		{ "FFBLS1", 9, 0, 0, 0, 0, 255, 255, 255, 255, 3, 8, 0, 2 },
		{ "FFLOV2", 6, 0, 0, 0, 0, 255, 255, 255, 255, 3, 5, 0, 2 },
		{ "FFWOE1", 11, 0, 0, 0, 0, 255, 255, 255, 255, 3, 9, 0, 2 },
		{ "FFSNF1", 9, 0, 0, 0, 0, 255, 255, 255, 255, 4, 6, 0, 4 },
		{ "FFLAF1", 9, 0, 0, 0, 0, 255, 255, 255, 255, 2, 8, 0, 1 },
		{ "FFSKE1", 11, 0, 0, 0, 0, 255, 255, 255, 255, 3, 10, 0, 2 },
		{ "RGTLK2", 10, 0, 0, 0, 0, 4, 6, 255, 255, 2, 6, 0, 1 },
		{ "RGTLK1", 10, 0, 0, 0, 0, 4, 6, 255, 255, 2, 6, 0, 1 },
		{ "BRTLK1", 26, 0, 0, 0, 0, 255, 255, 255, 255, 2, 23, 0, 255 },
		{ "BREXT1", 14, 0, 0, 0, 0, 255, 255, 255, 255, 255, 255, 0, 255 },
		{ "BRLRT1", 19, 0, 0, 0, 0, 255, 255, 255, 255, 1, 15, 0, 255 },
		{ "BRBWV1", 12, 0, 0, 0, 0, 255, 255, 255, 255, 3, 8, 0, 255 },
		{ "BRPAT1", 11, 0, 0, 0, 0, 255, 255, 255, 255, 255, 255, 0, 255 },
		{ "BRBSP1", 7, 0, 0, 0, 0, 255, 255, 255, 255, 255, 255, 0, 255 },
		{ "BRBEX1", 10, 0, 0, 0, 0, 255, 255, 255, 255, 255, 255, 0, 255 },
		{ "BRBLK1", 10, 0, 0, 0, 0, 255, 255, 255, 255, 255, 255, 0, 255 },
		{ "BRBET1", 17, 0, 0, 0, 0, 255, 255, 255, 255, 255, 255, 0, 255 },
		{ "BRWEX1", 10, 0, 0, 0, 0, 255, 255, 255, 255, 255, 255, 0, 255 },
		{ "BBTLK2", 26, 0, 0, 0, 0, 255, 255, 255, 255, 2, 23, 1, 255 },
		{ "BBEXT2", 14, 0, 0, 0, 0, 255, 255, 255, 255, 255, 255, 1, 255 },
		{ "BRLST1", 9, 0, 0, 0, 0, 255, 255, 255, 255, 2, 7, 0, 255 },
		{ "BRLSN1", 13, 0, 0, 0, 0, 255, 255, 255, 255, 1, 13, 2, 255 },
		{ "BRBNO1", 13, 0, 0, 0, 0, 255, 255, 255, 255, 255, 255, 0, 255 },
		{ "BRBND1", 8, 0, 0, 0, 0, 255, 255, 255, 255, 255, 255, 0, 255 },
		{ "BBLSN2", 13, 0, 0, 0, 0, 255, 255, 255, 255, 255, 255, 0, 255 },
		{ "CCTALK", 6, 0, 0, 0, 0, 2, 5, 255, 255, 1, 5, 0, 255 },
		{ "CCBIT1", 13, 0, 0, 0, 0, 3, 5, 9, 11, 2, 11, 2, 255 },
		{ "CCCMP1", 13, 0, 0, 0, 0, 6, 9, 255, 255, 5, 10, 1, 2 },
		{ "CCCOY1", 14, 0, 0, 0, 0, 6, 8, 255, 255, 4, 8, 0, 3 },
		{ "CCFNG1", 5, 0, 0, 0, 0, 255, 255, 255, 255, 4, 4, 0, 255 },
		{ "CCGRB1", 13, 0, 0, 0, 0, 6, 255, 255, 255, 6, 9, 0, 3 },
		{ "CCGST1", 9, 0, 0, 0, 0, 4, 255, 255, 255, 4, 7, 0, 2 },
		{ "CCHCN1", 10, 0, 0, 0, 0, 6, 9, 255, 255, 4, 9, 0, 0 },
		{ "CCHND1", 7, 0, 0, 0, 0, 6, 255, 255, 255, 2, 6, 0, 1 },
		{ "FTTLK2", 11, 0, 0, 0, 0, 1, 4, 6, 9, 1, 10, 0, 2 },
		{ "FTGNO2", 11, 0, 0, 0, 0, 4, 6, 8, 255, 4, 8, 1, 2 },
		{ "FTGST2", 6, 0, 0, 0, 0, 1, 2, 4, 5, 2, 5, 0, 1 },
		{ "FTHND2", 7, 0, 0, 0, 0, 2, 5, 255, 255, 1, 6, 1, 255 },
		{ "FTRNT2", 11, 0, 0, 0, 0, 3, 5, 7, 9, 2, 9, 1, 1 },
		{ "FTSRG2", 10, 0, 0, 0, 0, 4, 6, 8, 255, 3, 8, 1, 1 },
		{ "FTQOT2", 8, 0, 0, 0, 0, 1, 4, 8, 255, 1, 6, 1, 255 },
		{ "FMSTK1", 9, 0, 0, 0, 0, 255, 255, 255, 255, 1, 7, 0, 255 },
		{ "FMCRH1", 13, 0, 0, 0, 0, 255, 255, 255, 255, 3, 10, 0, 255 },
		{ "FMFGR1", 12, 0, 0, 0, 0, 255, 255, 255, 255, 1, 10, 0, 255 },
		{ "FMPRS1", 17, 0, 0, 0, 0, 255, 255, 255, 255, 1, 14, 0, 255 },
		{ "FMAGR1", 12, 0, 0, 0, 0, 255, 255, 255, 255, 2, 9, 0, 255 },
		{ "FMWOE1", 11, 0, 0, 0, 0, 255, 255, 255, 255, 1, 9, 0, 255 },
		{ "FMTOE1", 17, 0, 0, 0, 0, 255, 255, 255, 255, 255, 255, 0, 255 },
		{ "FM1TK1", 12, 0, 0, 0, 0, 255, 255, 255, 255, 255, 255, 0, 255 },
		{ "FM2TK1", 6, 0, 0, 0, 0, 255, 255, 255, 255, 255, 255, 0, 255 },
		{ "FM3TK1", 8, 0, 0, 0, 0, 255, 255, 255, 255, 255, 255, 0, 255 },
		{ "FMTNB1", 4, 0, 0, 0, 0, 255, 255, 255, 255, 255, 255, 0, 255 },
		{ "FMLOK1", 6, 0, 0, 0, 0, 255, 255, 255, 255, 255, 255, 0, 255 },
		{ "FMCST1", 11, 0, 0, 0, 0, 255, 255, 255, 255, 3, 8, 0, 255 },
		{ "FMLUP3", 8, 0, 0, 0, 0, 255, 255, 255, 255, 2, 5, 0, 255 },
		{ "BDTLK1", 8, 0, 0, 0, 0, 255, 255, 255, 255, 1, 7, 0, 255 },
		{ "BDGLE1", 15, 0, 0, 0, 0, 255, 255, 255, 255, 6, 10, 0, 255 },
		{ "BDSHK1", 16, 0, 0, 0, 0, 255, 255, 255, 255, 5, 11, 0, 1 },
		{ "BDWOE1", 22, 0, 0, 0, 0, 255, 255, 255, 255, 9, 16, 0, 2 },
		{ "BDHIP1", 22, 0, 0, 0, 0, 255, 255, 255, 255, 8, 16, 0, 1 },
		{ "BDFLG1", 13, 0, 0, 0, 0, 255, 255, 255, 255, 255, 255, 0, 255 },
		{ "BDKLT1", 12, 0, 0, 0, 0, 255, 255, 255, 255, 5, 10, 0, 255 },
		{ "BDSWY1", 8, 0, 0, 0, 0, 255, 255, 255, 255, 255, 255, 0, 255 },
		{ "WPSNK1", 5, 0, 0, 0, 0, 255, 255, 255, 255, 255, 255, 0, 255 },
		{ "WPLAF1", 10, 0, 0, 0, 0, 255, 255, 255, 255, 5, 9, 1, 1 },
		{ "DOTLK1", 10, 0, 0, 0, 0, 255, 255, 255, 255, 1, 8, 0, 255 },
		{ "DOGST1", 15, 0, 0, 0, 0, 255, 255, 255, 255, 4, 11, 1, 255 },
		{ "DO2DF1", 14, 0, 0, 0, 0, 255, 255, 255, 255, 3, 11, 1, 255 },
		{ "DOSNG1", 11, 0, 0, 0, 0, 255, 255, 255, 255, 8, 9, 1, 255 },
		{ "DOWOE1", 12, 0, 0, 0, 0, 255, 255, 255, 255, 5, 10, 1, 255 },
		{ "DO2ME1", 18, 0, 0, 0, 0, 255, 255, 255, 255, 5, 13, 1, 255 },
		{ "DOGLP1", 9, 0, 0, 0, 0, 255, 255, 255, 255, 255, 255, 1, 255 },
		{ "DOCRY1", 9, 0, 0, 0, 0, 255, 255, 255, 255, 3, 6, 1, 255 },
		{ "METLK1", 5, 0, 0, 0, 0, 255, 255, 255, 255, 1, 4, 0, 255 },
		{ "MECHT1", 10, 0, 0, 0, 0, 255, 255, 255, 255, 2, 9, 1, 255 },
		{ "ME2DF1", 10, 0, 0, 0, 0, 255, 255, 255, 255, 2, 9, 0, 255 },
		{ "MESNG1", 12, 0, 0, 0, 0, 255, 255, 255, 255, 5, 10, 2, 255 },
		{ "MEWOE1", 13, 0, 0, 0, 0, 255, 255, 255, 255, 3, 10, 1, 255 },
		{ "ME2DO1", 11, 0, 0, 0, 0, 255, 255, 255, 255, 2, 9, 1, 255 },
		{ "MEGLP1", 9, 0, 0, 0, 0, 255, 255, 255, 255, 255, 255, 1, 255 },
		{ "MECRY1", 12, 0, 0, 0, 0, 255, 255, 255, 255, 6, 9, 1, 255 },
		{ "CSTLK1", 8, 0, 0, 0, 0, 255, 255, 255, 255, 1, 7, 0, 0 },
		{ "CSNUD1", 14, 0, 0, 0, 0, 255, 255, 255, 255, 5, 11, 0, 2 },
		{ "CSSPR1", 11, 0, 0, 0, 0, 255, 255, 255, 255, 4, 8, 0, 2 },
		{ "CSWVE1", 13, 0, 0, 0, 0, 255, 255, 255, 255, 4, 9, 0, 1 },
		{ "CSYEL1", 9, 0, 0, 0, 0, 255, 255, 255, 255, 2, 6, 0, 1 },
		{ "JMTLK1", 7, 0, 0, 0, 0, 1, 4, 255, 255, 1, 6, 0, 0 },
		{ "JMEGO1", 11, 0, 0, 0, 0, 6, 255, 255, 255, 3, 8, 0, 1 },
		{ "JMARS1", 7, 0, 0, 0, 0, 4, 6, 255, 255, 3, 6, 0, 2 },
		{ "JMHIP1", 8, 0, 0, 0, 0, 3, 5, 7, 255, 2, 7, 0, 1 },
		{ "JMBNK1", 2, 0, 0, 0, 0, 255, 255, 255, 255, 255, 255, 0, 255 },
		{ "MRTLK1", 9, 0, 0, 0, 0, 4, 7, 255, 255, 2, 7, 0, 1 },
		{ "MRHOF1", 8, 0, 0, 0, 0, 3, 5, 255, 255, 2, 5, 0, 255 },
		{ "MRMRN1", 11, 0, 0, 0, 0, 3, 7, 255, 255, 1, 8, 0, 0 },
		{ "MRDPR1", 11, 0, 0, 0, 0, 1, 5, 9, 255, 1, 8, 0, 255 },
		{ "MRGLE1", 13, 0, 0, 0, 0, 5, 9, 255, 255, 3, 10, 0, 2 },
		{ "MRTDF1", 11, 0, 0, 0, 0, 3, 7, 9, 255, 3, 9, 0, 1 },
		{ "MREDF1", 11, 0, 0, 0, 0, 4, 255, 255, 255, 1, 10, 1, 255 },
		{ "MREPL1", 12, 0, 0, 0, 0, 5, 6, 7, 9, 2, 9, 1, 1 },
		{ "MRAPL1", 12, 0, 0, 0, 0, 4, 8, 9, 255, 2, 9, 0, 1 },
		{ "MREVL1", 8, 0, 0, 0, 0, 5, 255, 255, 255, 1, 5, 1, 255 },
		{ "BWDMR1", 16, 0, 0, 0, 0, 4, 7, 9, 11, 3, 14, 0, 1 },
		{ "BWBUF1", 12, 0, 0, 0, 0, 5, 8, 255, 255, 3, 11, 0, 1 },
		{ "BWHIP1", 12, 0, 0, 0, 0, 3, 6, 255, 255, 1, 9, 2, 0 },
		{ "BWHWL1", 14, 0, 0, 0, 0, 255, 255, 255, 255, 1, 4, 2, 255 },
		{ "BWLEN1", 10, 0, 0, 0, 0, 3, 6, 255, 255, 2, 7, 0, 1 },
		{ "BWSRL1", 6, 0, 0, 0, 0, 255, 255, 255, 255, 2, 5, 0, 1 },
		{ "BWWAG1", 6, 0, 0, 0, 0, 4, 10, 14, 18, 1, 4, 0, 0 },
		{ "BWYEL1", 8, 0, 0, 0, 0, 4, 255, 255, 255, 2, 7, 0, 1 },
		{ "BWTLK1", 15, 0, 0, 0, 0, 5, 8, 255, 255, 5, 9, 0, 1 },
		{ "SLTLK1", 19, 0, 0, 0, 0, 255, 255, 255, 255, 1, 18, 0, 255 },
		{ "SLPND1", 12, 0, 0, 0, 0, 255, 255, 255, 255, 255, 255, 0, 255 },
		{ "SLPNT1", 8, 0, 0, 0, 0, 255, 255, 255, 255, 255, 255, 0, 255 },
		{ "SLPTR1", 14, 0, 0, 0, 0, 255, 255, 255, 255, 6, 13, 1, 255 },
		{ "SDTLK1", 7, 0, 0, 0, 0, 255, 255, 255, 255, 1, 5, 0, 255 },
		{ "SDPDF1", 10, 0, 0, 0, 0, 255, 255, 255, 255, 3, 6, 0, 255 },
		{ "SDPNT1", 10, 0, 0, 0, 0, 255, 255, 255, 255, 2, 7, 0, 255 },
		{ "SDSLF1", 10, 0, 0, 0, 0, 255, 255, 255, 255, 2, 7, 0, 255 },
		{ "SDSTG1", 10, 0, 0, 0, 0, 255, 255, 255, 255, 255, 255, 0, 255 },
		{ "SDWVE1", 10, 0, 0, 0, 0, 255, 255, 255, 255, 1, 8, 0, 255 },
		{ "SDSTK1", 9, 0, 0, 0, 0, 255, 255, 255, 255, 255, 255, 0, 255 },
		{ "SDSMK1", 22, 0, 0, 0, 0, 255, 255, 255, 255, 255, 255, 0, 255 },
		{ "SDGLN1", 5, 0, 0, 0, 0, 255, 255, 255, 255, 255, 255, 0, 255 },
		{ "SDLAF1", 8, 0, 0, 0, 0, 255, 255, 255, 255, 255, 255, 0, 255 },
		{ "RMHIP1", 12, 0, 0, 0, 0, 7, 255, 255, 255, 1, 10, 2, 255 },
		{ "RMGES1", 19, 0, 0, 0, 0, 11, 255, 255, 255, 8, 13, 2, 2 },
		{ "RMPCH1", 18, 0, 0, 0, 0, 12, 255, 255, 255, 6, 13, 0, 2 },
		{ "RMSTH1", 12, 0, 0, 0, 0, 5, 255, 255, 255, 3, 6, 0, 2 },
		{ "RMHND1", 7, 0, 0, 0, 0, 5, 255, 255, 255, 5, 5, 1, 255 },
		{ "RMSTH1", 12, 0, 0, 0, 0, 5, 255, 255, 255, 5, 6, 1, 2 },
		{ "SGHND1", 10, 0, 0, 0, 0, 255, 255, 255, 255, 1, 9, 0, 0 },
		{ "SGSTF1", 13, 0, 0, 0, 0, 255, 255, 255, 255, 1, 12, 0, 255 },
		{ "SGSLP1", 16, 0, 0, 0, 0, 255, 255, 255, 255, 1, 15, 0, 255 },
		{ "SGPHC1", 12, 0, 0, 0, 0, 255, 255, 255, 255, 4, 9, 0, 255 },
		{ "SGHALT", 22, 0, 0, 0, 0, 255, 255, 255, 255, 7, 15, 0, 255 },
		{ "STTLK1", 13, 0, 0, 0, 0, 5, 9, 255, 255, 3, 10, 0, 2 },
		{ "STTNM1", 13, 0, 0, 0, 0, 5, 9, 255, 255, 3, 10, 0, 2 },
		{ "STFST1", 11, 0, 0, 0, 0, 3, 8, 255, 255, 1, 9, 0, 255 },
		{ "STLAF1", 20, 0, 0, 0, 0, 255, 255, 255, 255, 11, 15, 1, 2 },
		{ "STGES1", 13, 0, 0, 0, 0, 5, 7, 255, 255, 3, 7, 0, 2 },
		{ "STFNT1", 10, 0, 0, 0, 0, 4, 6, 255, 255, 255, 255, 0, 2 },
		{ "STSRK1", 10, 0, 0, 0, 0, 255, 255, 255, 255, 1, 3, 2, 0 },
		{ "STRED1", 11, 0, 0, 0, 0, 255, 255, 255, 255, 2, 10, 0, 255 },
		{ "STLKU1", 6, 0, 0, 0, 0, 3, 255, 255, 255, 2, 5, 0, 0 },
		{ "STKEY1", 15, 0, 0, 0, 0, 9, 11, 255, 255, 9, 14, 0, 255 },
		{ "STMKTD1", 7, 0, 0, 0, 0, 3, 6, 255, 255, 1, 6, 0, 255 },
		{ "STTKM1", 21, 0, 0, 0, 0, 12, 13, 15, 16, 12, 17, 0, 1 },
		{ "STMSZ1", 10, 0, 0, 0, 0, 255, 255, 255, 255, 1, 3, 2, 255 },
		{ "STPNV1", 14, 0, 0, 0, 0, 6, 11, 255, 255, 4, 11, 0, 1 },
		{ "STSOM1", 10, 0, 0, 0, 0, 255, 255, 255, 255, 1, 3, 2, 255 },
		{ "MYTLK1", 9, 0, 0, 0, 0, 2, 4, 255, 255, 1, 4, 0, 0 },
		{ "MYSQUAWK", 5, 0, 0, 0, 0, 255, 255, 255, 255, 3, 3, 1, 255 },
		{ "SPTLK", 12, 0, 0, 0, 0, 255, 255, 255, 255, 255, 255, 0, 255 },
		{ "SPARM", 16, 0, 0, 0, 0, 255, 255, 255, 255, 255, 255, 0, 255 },
		{ "SPHOP", 18, 0, 0, 0, 0, 255, 255, 255, 255, 255, 255, 0, 255 },
		{ "SPLNT", 16, 0, 0, 0, 0, 255, 255, 255, 255, 3, 13, 0, 255 },
		{ "SPLAF", 11, 0, 0, 0, 0, 255, 255, 255, 255, 5, 10, 2, 255 },
		{ "SPTFN", 10, 0, 0, 0, 0, 255, 255, 255, 255, 255, 255, 0, 255 },
		{ "SPPIN", 14, 0, 0, 0, 0, 255, 255, 255, 255, 255, 255, 0, 255 },
		{ "SPINH1", 21, 0, 0, 0, 0, 255, 255, 255, 255, 255, 255, 0, 255 },
		{ "SPSFTCOM", 10, 0, 0, 0, 0, 255, 255, 255, 255, 255, 255, 0, 255 },
		{ "MFTMZ1", 9, 0, 0, 0, 0, 255, 255, 255, 255, 1, 8, 1, 255 },
		{ "MFTLK1", 13, 0, 0, 0, 0, 2, 7, 255, 255, 1, 12, 1, 255 },
		{ "VGCIR1", 15, 0, 0, 0, 0, 5, 9, 255, 255, 2, 13, 1, 255 },
		{ "VGBIT1", 12, 0, 0, 0, 0, 6, 9, 255, 255, 2, 9, 1, 255 },
		{ "VGANG1", 10, 0, 0, 0, 0, 9, 255, 255, 255, 1, 9, 0, 255 },
		{ "VGCOM1", 13, 0, 0, 0, 0, 5, 11, 255, 255, 2, 11, 0, 255 },
		{ "VGCUR1", 8, 0, 0, 0, 0, 4, 8, 255, 255, 2, 7, 0, 255 },
		{ "VGTLK1", 11, 0, 0, 0, 0, 3, 6, 255, 255, 3, 10, 0, 255 },
		{ "VGEXP1", 10, 0, 0, 0, 0, 5, 9, 255, 255, 3, 9, 0, 255 },
		{ "WFTLK1", 8, 0, 0, 0, 0, 255, 255, 255, 255, 1, 7, 0, 1 },
		{ "WFPNT1", 20, 0, 0, 0, 0, 255, 255, 255, 255, 6, 16, 0, 1 },
		{ "WFFST1", 13, 0, 0, 0, 0, 255, 255, 255, 255, 2, 8, 0, 2 },
		{ "WFTNO1", 8, 0, 0, 0, 0, 255, 255, 255, 255, 2, 5, 0, 2 },
		{ "WFSRG1", 11, 0, 0, 0, 0, 255, 255, 255, 255, 3, 8, 0, 1 },
		{ "WFGTK1", 16, 0, 0, 0, 0, 255, 255, 255, 255, 1, 15, 0, 255 },
		{ "WFPAW1", 24, 0, 0, 0, 0, 255, 255, 255, 255, 4, 22, 0, 1 },
		{ "LGTLK", 20, 0, 0, 0, 0, 4, 8, 11, 15, 1, 17, 0, 255 },
		{ "LGSHOUT", 16, 0, 0, 0, 0, 12, 255, 255, 255, 6, 12, 0, 255 },
		{ "POMRN1", 12, 0, 0, 0, 0, 3, 5, 7, 255, 3, 9, 0, 2 },
		{ "POGLE1", 14, 0, 0, 0, 0, 7, 10, 255, 255, 5, 10, 0, 2 },
		{ "PLMRG1", 16, 0, 0, 0, 0, 9, 255, 255, 255, 8, 12, 0, 1 },
		{ "PLCMR1", 16, 0, 0, 0, 0, 8, 10, 255, 255, 8, 12, 0, 3 },
		{ "PLEVL1", 17, 0, 0, 0, 0, 9, 255, 255, 255, 7, 9, 0, 1 },
		{ "PLEDF1", 9, 0, 0, 0, 0, 4, 6, 255, 255, 5, 7, 0, 2 },
		{ "PLTLK1", 11, 0, 0, 0, 0, 5, 8, 255, 255, 5, 8, 0, 1 },
		{ "ELTLK1", 8, 0, 0, 0, 0, 3, 5, 7, 255, 2, 7, 0, 255 },
		{ "ELSNR1", 7, 0, 0, 0, 0, 3, 255, 255, 255, 1, 5, 0, 255 },
		{ "RG2TK1", 10, 0, 0, 0, 0, 4, 6, 255, 255, 2, 6, 0, 1 },
		{ "RG2TK1", 10, 0, 0, 0, 0, 4, 6, 255, 255, 2, 6, 0, 1 },
		{ "C2TALK", 6, 0, 0, 0, 0, 2, 5, 255, 255, 1, 5, 0, 255 },
		{ "C2BIT1", 13, 0, 0, 0, 0, 3, 5, 9, 11, 2, 11, 2, 255 },
		{ "C2CMP1", 13, 0, 0, 0, 0, 6, 9, 255, 255, 5, 10, 1, 2 },
		{ "C2COY1", 14, 0, 0, 0, 0, 6, 8, 255, 255, 4, 8, 0, 3 },
		{ "C2FNG1", 5, 0, 0, 0, 0, 255, 255, 255, 255, 4, 4, 0, 255 },
		{ "C2GRB1", 13, 0, 0, 0, 0, 6, 255, 255, 255, 6, 9, 0, 3 },
		{ "C2GST1", 9, 0, 0, 0, 0, 4, 255, 255, 255, 4, 7, 0, 2 },
		{ "C2HCN1", 10, 0, 0, 0, 0, 6, 9, 255, 255, 4, 9, 0, 0 },
		{ "C2HND1", 7, 0, 0, 0, 0, 6, 255, 255, 255, 2, 6, 0, 1 },
		{ "666TKBB3", 21, 0, 0, 0, 0, 9, 14, 255, 255, 6, 16, 0, 255 },
		{ "665TFLX3", 27, 0, 0, 0, 0, 10, 14, 17, 255, 10, 18, 0, 255 },
		{ "664FXTK3", 18, 0, 0, 0, 0, 5, 7, 11, 13, 3, 15, 0, 255 },
		{ "FDTALK", 15, 0, 0, 0, 0, 9, 255, 255, 255, 7, 9, 0, 255 },
		{ "FDYELL", 16, 0, 0, 0, 0, 10, 255, 255, 255, 8, 10, 0, 255 },
		{ "GLTLK", 20, 0, 0, 0, 0, 6, 12, 18, 255, 1, 19, 0, 255 },
		{ "GLTRN", 4, 0, 0, 0, 0, 3, 255, 255, 255, 1, 2, 0, 255 },
		{ "RAYTALK1", 10, 0, 0, 0, 0, 3, 5, 8, 255, 1, 9, 0, 255 },
		{ "BRTKB1", 17, 0, 0, 0, 0, 255, 255, 255, 255, 2, 14, 0, 255 }
	};

	static const int32 characterAnims[] = {
		0,   39,  81,  89,  91,  108, 117, 124, 138, 146,
		148, 156, 164, 169, 174, 179, 184, 193, 197, 207,
		213, 218, 233, 235, 244, 245, 246, 246, 246, 246,
		253, 253, 253, 253, 260, 262, 264, 269, 271, 273,
		282, 284, 285, 287, 289, 290, 291, 291, 291, 291,
		289, 289, 289, 289, 289, 289, 289, 289, 289, 289
	};

	return &anims[characterAnims[characterId] + animationId];
}

bool Character::loadShadowAnimation(Common::String animName) {
	debugC(1, kDebugCharacter, "loadShadowAnimation(%s)", animName.c_str());

	_shadowAnim = new Animation(_vm);
	if (!_shadowAnim->loadAnimation(animName))
		return false;

	_shadowAnimationInstance = _vm->getAnimationManager()->createNewInstance(kAnimationCharacter);
	_vm->getAnimationManager()->addInstance(_shadowAnimationInstance);
	_shadowAnimationInstance->setAnimation(_shadowAnim);
	_shadowAnimationInstance->setVisible(true);
	_shadowAnimationInstance->setUseMask(true);

	return true;
}

void Character::playAnim(int32 animId, int32 unused, int32 flags) {
	debugC(3, kDebugCharacter, "playAnim(%d, unused, %d)", animId, flags);

	if (animId == 0)
		animId = _animSpecialDefaultId;

	// get the anim to load
	const SpecialCharacterAnimation *anim = getSpecialAnimation(_id, animId);

	char animName[20];
	strcpy(animName, anim->_filename);
	if (strchr(animName, '?'))
		*strchr(animName, '?') = '0' + _facing;
	strcat(animName, ".CAF");


	if (_animScriptId != -1)
		_vm->getSceneAnimationScript(_animScriptId)->_frozen = true;

	if (_sceneAnimationId > -1)
		setAnimationInstance(_vm->getSceneAnimation(_sceneAnimationId)->_animInstance);

	stopSpecialAnim();

	if (flags & 8) {
		// talker
		_lineToSayId = _vm->getCurrentLineToSay();

		// make the talker busy
		_flags |= 1;
	}
	_animFlags |= flags;

	if (_specialAnim)
		delete _specialAnim;
	_specialAnim = new Animation(_vm);
	_specialAnim->loadAnimation(animName);

	_animSpecialId = animId;

	_animationInstance->setAnimation(_specialAnim);
	_animationInstance->setAnimationRange(0, _specialAnim->_numFrames - 1);
	_animationInstance->reset();
	_animationInstance->stopAnimation();
	_animationInstance->setLooping(false);
}

int32 Character::getAnimFlag() {
	return _animFlags;
}

void Character::setAnimFlag(int32 flag) {
	_animFlags = flag;
}

int32 Character::getSceneAnimationId() {
	return _sceneAnimationId;
}

void Character::setDefaultSpecialAnimationId(int32 defaultAnimationId) {
	_animSpecialDefaultId = defaultAnimationId;
}

int32 Character::getFinalX() {
	return _finalX;
}

int32 Character::getFinalY() {
	return _finalY;
}

void Character::updateIdle() {
	debugC(5, kDebugCharacter, "updateIdle()");

	// only flux and drew
	if (_id > 1)
		return;

	if (_vm->state()->_mouseHidden)
		_nextIdleTime = _vm->getOldMilli() + (300 + _vm->randRange(0, 600)) * _vm->getTickLength();

	if (_vm->getOldMilli() > _nextIdleTime) {
		if (((_flags & 1) == 0) || ((_flags & 2) != 0)) {
			if (!_vm->state()->_inCloseUp && !_vm->state()->_inCutaway && _animSpecialId == -1) {
				if (!_vm->state()->_mouseHidden) {
					_nextIdleTime = _vm->getOldMilli() + (300 + _vm->randRange(0, 600)) * _vm->getTickLength();
					playAnim(getRandomIdleAnim(), 0, 0x40);
					_flags |= 0x4;
				}
			}
		}
	}
}
} // End of namespace Toon

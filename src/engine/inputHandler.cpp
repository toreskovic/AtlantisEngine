#include "inputHandler.h"

// alias for raylib's IsKeyDown
constexpr auto RaylibIsKeyDown = IsKeyDown;

// alias for raylib's GetKeyPressed
constexpr auto RaylibGetKeyPressed = GetKeyPressed;

namespace Atlantis
{
void AInputHandler::SyncInput()
{
    // get all released keys
    _keysReleased.clear();
    for (int i = _keysPressed.size() - 1; i >= 0; --i)
    {
        if (!RaylibIsKeyDown(_keysPressed[i]))
        {
            _keysReleased.push_back(_keysPressed[i]);
            _keysPressed.erase(_keysPressed.begin() + i);
        }
    }

    // get all pressed keys
    _keysJustPressed.clear();
    while (int key = RaylibGetKeyPressed())
    {
        // check if key is already pressed
        bool alreadyPressed = false;
        for (int i = 0; i < _keysPressed.size(); ++i)
        {
            if (_keysPressed[i] == key)
            {
                alreadyPressed = true;
                break;
            }
        }

        if (!alreadyPressed)
        {
            _keysJustPressed.push_back(static_cast<KeyboardKey>(key));
            _keysPressed.push_back(static_cast<KeyboardKey>(key));
        }
    }
}

bool AInputHandler::IsKeyDown(KeyboardKey key) const
{
    return std::find(_keysPressed.begin(), _keysPressed.end(), key) != _keysPressed.end();
}

bool AInputHandler::IsKeyPressed(KeyboardKey key) const
{
    return std::find(_keysJustPressed.begin(), _keysJustPressed.end(), key) != _keysJustPressed.end();
}

bool AInputHandler::IsKeyReleased(KeyboardKey key) const
{
    return std::find(_keysReleased.begin(), _keysReleased.end(), key) != _keysReleased.end();
}

}
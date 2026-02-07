#ifndef ATLANTIS_INPUT_HANDLER_H
#define ATLANTIS_INPUT_HANDLER_H

#include "raylib.h"
#include <vector>

namespace Atlantis
{
    class AInputHandler
    {
    public:
        void SyncInput();

        bool IsKeyDown(KeyboardKey key) const;
        bool IsKeyPressed(KeyboardKey key) const;
        bool IsKeyReleased(KeyboardKey key) const;
    private:
        // keyboard main thread
        std::vector<KeyboardKey> _keysPressed;
        std::vector<KeyboardKey> _keysJustPressed;
        std::vector<KeyboardKey> _keysReleased;

        // mouse main thread
        std::vector<MouseButton> _mouseButtonsPressed;
        std::vector<MouseButton> _mouseButtonsReleased;

        // gamepad main thread
        std::vector<GamepadButton> _gamepadButtonsPressed;
        std::vector<GamepadButton> _gamepadButtonsReleased;
        std::vector<GamepadAxis> _gamepadAxis;
    };
}

#endif //ATLANTIS_INPUT_HANDLER_H
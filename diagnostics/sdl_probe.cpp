#include <SDL3/SDL.h>
#include <iostream>

static void printGamepads()
{
    int count = 0;
    SDL_JoystickID* gamepads = SDL_GetGamepads(&count);

    std::cout << "gamepads=" << count << "\n";

    for (int i = 0; gamepads && i < count; i++) {
        SDL_JoystickID id = gamepads[i];
        std::cout
            << "gamepad[" << i << "]"
            << " name=" << (SDL_GetGamepadNameForID(id) ? SDL_GetGamepadNameForID(id) : "(null)")
            << " vendor=0x" << std::hex << SDL_GetGamepadVendorForID(id)
            << " product=0x" << SDL_GetGamepadProductForID(id)
            << std::dec << "\n";
    }

    if (gamepads) {
        SDL_free(gamepads);
    }
}

static void printJoysticks()
{
    int count = 0;
    SDL_JoystickID* joysticks = SDL_GetJoysticks(&count);

    std::cout << "joysticks=" << count << "\n";

    for (int i = 0; joysticks && i < count; i++) {
        SDL_JoystickID id = joysticks[i];
        std::cout
            << "joystick[" << i << "]"
            << " name=" << (SDL_GetJoystickNameForID(id) ? SDL_GetJoystickNameForID(id) : "(null)")
            << " vendor=0x" << std::hex << SDL_GetJoystickVendorForID(id)
            << " product=0x" << SDL_GetJoystickProductForID(id)
            << std::dec << "\n";
    }

    if (joysticks) {
        SDL_free(joysticks);
    }
}

int main()
{
    SDL_SetHint(SDL_HINT_AUTO_UPDATE_JOYSTICKS, "1");
    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
    SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI, "1");
    SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_PS4, "1");
    SDL_SetHint(SDL_HINT_JOYSTICK_RAWINPUT, "1");
    SDL_SetHint(SDL_HINT_JOYSTICK_WGI, "1");
    SDL_SetHint(SDL_HINT_WINDOWS_GAMEINPUT, "1");
    SDL_SetHint(SDL_HINT_XINPUT_ENABLED, "1");

    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD | SDL_INIT_JOYSTICK)) {
        std::cerr << "SDL_Init failed: " << SDL_GetError() << "\n";
        return 1;
    }

    std::cout << "SDL initialized\n";

    for (int pass = 0; pass < 8; pass++) {
        SDL_PumpEvents();
        SDL_UpdateGamepads();
        SDL_UpdateJoysticks();

        std::cout << "pass=" << pass << "\n";
        printGamepads();
        printJoysticks();

        SDL_Delay(1000);
    }

    SDL_Quit();
    return 0;
}

#include <SDL.h>
#include <SDL_image.h>
#include <SDL_ttf.h>
#include <iostream>
#include <string>
#include <vector>

const int SCREEN_WIDTH = 800;
const int SCREEN_HEIGHT = 600;
const float GRAVITY = 0.2f;
const float JUMP_SPEED = -6.0f;
const int GROUND_LEVEL = SCREEN_HEIGHT - 50;
const int PLATFORM_HEIGHT = 20;
const int PLATFORM_WIDTH = 150;

enum class GameState
{
    MainMenu,
    MapScreen,
    SkinScreen,
    GameScreen,
    WinScreen,
    LoseScreen
};

struct AudioData
{
    Uint8 *pos;
    Uint32 length;
};

void MyAudioCallback(void *userdata, Uint8 *stream, int len)
{
    AudioData *audio = (AudioData *)userdata;

    if (audio->length == 0)
        return;

    len = (len > audio->length ? audio->length : len);
    SDL_memcpy(stream, audio->pos, len);

    audio->pos += len;
    audio->length -= len;
}

SDL_Texture *loadTexture(SDL_Renderer *renderer, const std::string &path)
{
    SDL_Surface *loadedSurface = IMG_Load(path.c_str());
    if (!loadedSurface)
    {
        std::cerr << "Unable to load image " << path << "! SDL_image Error: " << IMG_GetError() << std::endl;
        return nullptr;
    }
    SDL_Texture *newTexture = SDL_CreateTextureFromSurface(renderer, loadedSurface);
    SDL_FreeSurface(loadedSurface);
    if (!newTexture)
    {
        std::cerr << "Unable to create texture from " << path << "! SDL Error: " << SDL_GetError() << std::endl;
    }
    return newTexture;
}

class Platform
{
public:
    Platform(int x, int y, int w, int h)
        : rect{x, y, w, h} {}

    void draw(SDL_Renderer *renderer) const
    {
        SDL_SetRenderDrawColor(renderer, 255, 229, 204, 255);
        SDL_RenderFillRect(renderer, &rect);
    }

    SDL_Rect getRect() const
    {
        return rect;
    }

private:
    SDL_Rect rect;
};

class Button
{
public:
    Button(SDL_Renderer *renderer, const std::string &text, int x, int y, int w, int h)
        : renderer(renderer), text(text), rect{x, y, w, h}, isMouseOver(false) {}

    void draw()
    {
        SDL_SetRenderDrawColor(renderer, isMouseOver ? 100 : 50, 50, 50, 255);
        SDL_RenderFillRect(renderer, &rect);

        SDL_Color textColor = {255, 255, 255};
        TTF_Font *font = TTF_OpenFont("fonts/pixeloid-font/PixeloidMono-d94EV.ttf", 24);
        SDL_Surface *textSurface = TTF_RenderText_Solid(font, text.c_str(), textColor);
        SDL_Texture *textTexture = SDL_CreateTextureFromSurface(renderer, textSurface);
        SDL_FreeSurface(textSurface);
        int textW, textH;
        SDL_QueryTexture(textTexture, NULL, NULL, &textW, &textH);
        SDL_Rect textRect = {rect.x + rect.w / 2 - textW / 2, rect.y + rect.h / 2 - textH / 2, textW, textH};
        SDL_RenderCopy(renderer, textTexture, NULL, &textRect);
        SDL_DestroyTexture(textTexture);
        TTF_CloseFont(font);
    }

    bool containsPoint(int x, int y) const
    {
        return x >= rect.x && x <= rect.x + rect.w && y >= rect.y && y <= rect.y + rect.h;
    }

    void setMouseOver(bool mouseOver) { isMouseOver = mouseOver; }

private:
    SDL_Renderer *renderer;
    std::string text;
    SDL_Rect rect;
    bool isMouseOver;
};

class Player
{
public:
    Player(int x, int y, int w, int h, SDL_Texture *texture, bool onGround)
        : rect{x, y, w, h}, texture(texture), speed(3), velY(0), onGround(onGround) {}

    void update(const Uint8 *state, GameState &gameState, const std::vector<Platform> &platforms)
    {
        int moveX = 0;
        if (state[SDL_SCANCODE_A])
            moveX -= speed;
        if (state[SDL_SCANCODE_D])
            moveX += speed;

        rect.x += moveX;

        if (rect.x < 0)
            rect.x = 0;
        if (rect.x + rect.w > SCREEN_WIDTH)
            rect.x = SCREEN_WIDTH - rect.w;

        velY += GRAVITY;
        rect.y += static_cast<int>(velY);
        SDL_Delay(5);

        onGround = false;
        for (const auto &platform : platforms)
        {
            SDL_Rect platformRect = platform.getRect();
            if (rect.y + rect.h >= platformRect.y && rect.y + rect.h <= platformRect.y + platformRect.h &&
                rect.x + rect.w > platformRect.x && rect.x < platformRect.x + platformRect.w)
            {
                rect.y = platformRect.y - rect.h;
                onGround = true;
                velY = 0;
                break;
            }
        }

        if (rect.y + rect.h >= GROUND_LEVEL)
        {
            rect.y = GROUND_LEVEL - rect.h;
            onGround = true;
            velY = 0;
            gameState = GameState::LoseScreen;
        }

        if (rect.x + rect.w >= SCREEN_WIDTH)
        {
            gameState = GameState::WinScreen;
        }

        if (onGround && state[SDL_SCANCODE_W])
        {
            onGround = false;
            velY = JUMP_SPEED;
        }
    }

    void draw(SDL_Renderer *renderer)
    {
        SDL_RenderCopy(renderer, texture, nullptr, &rect);
    }

    void setTexture(SDL_Texture *newTexture)
    {
        texture = newTexture;
    }

private:
    SDL_Rect rect;
    SDL_Texture *texture;
    int speed;
    float velY;
    bool onGround;
    bool onAnyPlatform;
};

void drawPlatform(SDL_Renderer *renderer)
{
    SDL_Rect platformRect = {0, GROUND_LEVEL - PLATFORM_HEIGHT, PLATFORM_WIDTH, PLATFORM_HEIGHT};
    SDL_RenderFillRect(renderer, &platformRect);
}

void runGameLoop(SDL_Renderer *renderer)
{
    GameState gameState = GameState::MainMenu;
    bool isRunning = true;
    SDL_Event event;

    SDL_Texture *backgroundTextureMainMenu = loadTexture(renderer, "images/london.bmp");
    SDL_Texture *backgroundTextureGameScreen = loadTexture(renderer, "images/london.bmp");
    SDL_Texture *backgroundTextureMapScreen = loadTexture(renderer, "images/london.bmp");

    Button playButton(renderer, "PLAY", SCREEN_WIDTH / 2 - 100, SCREEN_HEIGHT / 2 - 150, 200, 50);
    Button skinButton(renderer, "SKINS", SCREEN_WIDTH / 2 - 225, SCREEN_HEIGHT / 2 - 75, 200, 50);
    Button mapButton(renderer, "MAP", SCREEN_WIDTH / 2 + 25, SCREEN_HEIGHT / 2 - 75, 200, 50);
    Button exitButton(renderer, "EXIT", SCREEN_WIDTH / 2 + 150, SCREEN_HEIGHT / 2 + 200, 200, 50);
    Button selectButton(renderer, "SELECT", SCREEN_WIDTH / 2 - 100, SCREEN_HEIGHT / 2 + 125, 200, 50);
    Button backButton(renderer, "BACK", SCREEN_WIDTH / 2 - 100, SCREEN_HEIGHT / 2 + 200, 200, 50);
    Button winBackButton(renderer, "BACK TO MENU", SCREEN_WIDTH / 2 - 100, SCREEN_HEIGHT / 2 + 100, 200, 50);
    Button loseBackButton(renderer, "TRY AGAIN", SCREEN_WIDTH / 2 - 100, SCREEN_HEIGHT / 2 + 100, 200, 50);

    Button leftArrowButton(renderer, "<", SCREEN_WIDTH / 2 - 250, SCREEN_HEIGHT / 2, 50, 50);
    Button rightArrowButton(renderer, ">", SCREEN_WIDTH / 2 + 200, SCREEN_HEIGHT / 2, 50, 50);

    std::vector<std::string> mapImagePaths = {
        "images/nyc.bmp",
        "images/sydney.bmp",
        "images/london.bmp",
        "images/pisa.bmp",
        "images/moai.bmp",
        "images/pjatk.bmp"};

    std::vector<std::string> skinImagePaths = {
        "skins/dziekan.bmp",
        "skins/rektor.bmp"};

    std::vector<Platform> platforms = {
        Platform(0, GROUND_LEVEL - PLATFORM_HEIGHT, PLATFORM_WIDTH, PLATFORM_HEIGHT),
        Platform(200, GROUND_LEVEL - 100, PLATFORM_WIDTH, PLATFORM_HEIGHT),
        Platform(0, GROUND_LEVEL - 160, PLATFORM_WIDTH, PLATFORM_HEIGHT),
        Platform(200, GROUND_LEVEL - 240, PLATFORM_WIDTH, PLATFORM_HEIGHT),
        Platform(0, GROUND_LEVEL - 320, PLATFORM_WIDTH, PLATFORM_HEIGHT),
        Platform(200, GROUND_LEVEL - 400, PLATFORM_WIDTH, PLATFORM_HEIGHT),
        Platform(400, GROUND_LEVEL - 480, PLATFORM_WIDTH, PLATFORM_HEIGHT),
        Platform(600, GROUND_LEVEL - 160, PLATFORM_WIDTH, PLATFORM_HEIGHT),
    };

    int currentMapIndex = 0;
    int currentSkinIndex = 0;

    SDL_Texture *mapThumbnailTexture = loadTexture(renderer, mapImagePaths[currentMapIndex]);
    SDL_Texture *skinThumbnailTexture = loadTexture(renderer, skinImagePaths[currentSkinIndex]);

    SDL_Texture *playerTexture = loadTexture(renderer, skinImagePaths[currentSkinIndex]);
    Player player(0, GROUND_LEVEL - PLATFORM_HEIGHT - 50, 50, 50, playerTexture, true);

    SDL_AudioSpec wavSpec;
    Uint8 *wavStart;
    Uint32 wavLength;
    if (SDL_LoadWAV("music/test.wav", &wavSpec, &wavStart, &wavLength) == nullptr)
    {
        std::cerr << "Failed to load WAV file! SDL Error: " << SDL_GetError() << std::endl;
        return;
    }
    AudioData audioData = {wavStart, wavLength};
    wavSpec.callback = MyAudioCallback;
    wavSpec.userdata = &audioData;

    SDL_AudioDeviceID audioDevice = SDL_OpenAudioDevice(NULL, 0, &wavSpec, NULL, 0);
    if (audioDevice == 0)
    {
        std::cerr << "Failed to open audio device! SDL Error: " << SDL_GetError() << std::endl;
        SDL_FreeWAV(wavStart);
        return;
    }

    while (isRunning)
    {
        while (SDL_PollEvent(&event))
        {
            if (event.type == SDL_QUIT)
            {
                isRunning = false;
            }
            else if (event.type == SDL_MOUSEBUTTONDOWN)
            {
                int mouseX = event.button.x;
                int mouseY = event.button.y;

                switch (gameState)
                {
                case GameState::MainMenu:
                    if (playButton.containsPoint(mouseX, mouseY))
                    {
                        gameState = GameState::GameScreen;
                        SDL_PauseAudioDevice(audioDevice, 0);
                    }
                    else if (skinButton.containsPoint(mouseX, mouseY))
                    {
                        gameState = GameState::SkinScreen;
                    }
                    else if (mapButton.containsPoint(mouseX, mouseY))
                    {
                        gameState = GameState::MapScreen;
                    }
                    else if (exitButton.containsPoint(mouseX, mouseY))
                    {
                        isRunning = false;
                    }
                    break;
                case GameState::SkinScreen:
                    if (leftArrowButton.containsPoint(mouseX, mouseY))
                    {
                        currentSkinIndex = (currentSkinIndex - 1 + skinImagePaths.size()) % skinImagePaths.size();
                        skinThumbnailTexture = loadTexture(renderer, skinImagePaths[currentSkinIndex]);
                    }
                    else if (rightArrowButton.containsPoint(mouseX, mouseY))
                    {
                        currentSkinIndex = (currentSkinIndex + 1) % skinImagePaths.size();
                        skinThumbnailTexture = loadTexture(renderer, skinImagePaths[currentSkinIndex]);
                    }
                    else if (selectButton.containsPoint(mouseX, mouseY))
                    {
                        playerTexture = loadTexture(renderer, skinImagePaths[currentSkinIndex]);
                        player.setTexture(playerTexture);
                        std::cout << "Wybrano skina!" << std::endl;
                    }
                    else if (backButton.containsPoint(mouseX, mouseY))
                    {
                        gameState = GameState::MainMenu;
                    }
                    break;
                case GameState::MapScreen:
                    if (leftArrowButton.containsPoint(mouseX, mouseY))
                    {
                        currentMapIndex = (currentMapIndex - 1 + mapImagePaths.size()) % mapImagePaths.size();
                        mapThumbnailTexture = loadTexture(renderer, mapImagePaths[currentMapIndex]);
                    }
                    else if (rightArrowButton.containsPoint(mouseX, mouseY))
                    {
                        currentMapIndex = (currentMapIndex + 1) % mapImagePaths.size();
                        mapThumbnailTexture = loadTexture(renderer, mapImagePaths[currentMapIndex]);
                    }
                    else if (selectButton.containsPoint(mouseX, mouseY))
                    {
                        std::cout << "Wybrano mapę!" << std::endl;
                        std::string selectedMapPath = mapImagePaths[currentMapIndex];
                        SDL_DestroyTexture(backgroundTextureMapScreen);
                        backgroundTextureMapScreen = loadTexture(renderer, selectedMapPath);
                        backgroundTextureMainMenu = loadTexture(renderer, selectedMapPath);
                        backgroundTextureGameScreen = loadTexture(renderer, selectedMapPath);
                    }
                    else if (backButton.containsPoint(mouseX, mouseY))
                    {
                        gameState = GameState::MainMenu;
                    }
                    break;
                case GameState::WinScreen:
                    if (winBackButton.containsPoint(mouseX, mouseY))
                    {
                        gameState = GameState::MainMenu;
                        player = Player(0, GROUND_LEVEL - PLATFORM_HEIGHT - 50, 50, 50, playerTexture, true);
                    }
                    break;
                case GameState::LoseScreen:
                    if (loseBackButton.containsPoint(mouseX, mouseY))
                    {
                        gameState = GameState::GameScreen;
                        player = Player(0, GROUND_LEVEL - PLATFORM_HEIGHT - 50, 50, 50, playerTexture, true);
                    }
                    break;
                default:
                    break;
                }
            }
            else if (event.type == SDL_KEYDOWN)
            {
                if (event.key.keysym.sym == SDLK_ESCAPE)
                {
                    if (gameState == GameState::GameScreen)
                    {
                        gameState = GameState::MainMenu;
                        SDL_PauseAudioDevice(audioDevice, 1);
                        player = Player(0, GROUND_LEVEL - PLATFORM_HEIGHT - 50, 50, 50, playerTexture, true);
                    }
                }
            }
            else if (event.type == SDL_MOUSEMOTION)
            {
                int mouseX = event.motion.x;
                int mouseY = event.motion.y;

                switch (gameState)
                {
                case GameState::MainMenu:
                    playButton.setMouseOver(playButton.containsPoint(mouseX, mouseY));
                    skinButton.setMouseOver(skinButton.containsPoint(mouseX, mouseY));
                    mapButton.setMouseOver(mapButton.containsPoint(mouseX, mouseY));
                    exitButton.setMouseOver(exitButton.containsPoint(mouseX, mouseY));
                    break;
                case GameState::MapScreen:
                    leftArrowButton.setMouseOver(leftArrowButton.containsPoint(mouseX, mouseY));
                    rightArrowButton.setMouseOver(rightArrowButton.containsPoint(mouseX, mouseY));
                    selectButton.setMouseOver(selectButton.containsPoint(mouseX, mouseY));
                    backButton.setMouseOver(backButton.containsPoint(mouseX, mouseY));
                    break;
                case GameState::SkinScreen:
                    leftArrowButton.setMouseOver(leftArrowButton.containsPoint(mouseX, mouseY));
                    rightArrowButton.setMouseOver(rightArrowButton.containsPoint(mouseX, mouseY));
                    selectButton.setMouseOver(selectButton.containsPoint(mouseX, mouseY));
                    backButton.setMouseOver(backButton.containsPoint(mouseX, mouseY));
                    break;
                case GameState::WinScreen:
                    winBackButton.setMouseOver(winBackButton.containsPoint(mouseX, mouseY));
                    break;
                case GameState::LoseScreen:
                    loseBackButton.setMouseOver(loseBackButton.containsPoint(mouseX, mouseY));
                    break;
                default:
                    break;
                }
            }
        }

        SDL_RenderClear(renderer);

        const Uint8 *state = SDL_GetKeyboardState(NULL);

        SDL_Rect mapThumbnailRect = {SCREEN_WIDTH / 2 - 100, SCREEN_HEIGHT / 2 - 100, 200, 200};
        SDL_Rect skinThumbnailRect = {SCREEN_WIDTH / 2 - 100, SCREEN_HEIGHT / 2 - 100, 200, 200};

        switch (gameState)
        {
        case GameState::MainMenu:
            SDL_RenderCopy(renderer, backgroundTextureMainMenu, nullptr, nullptr);
            playButton.draw();
            skinButton.draw();
            mapButton.draw();
            exitButton.draw();
            break;
        case GameState::MapScreen:
            SDL_RenderCopy(renderer, backgroundTextureMapScreen, nullptr, nullptr);
            SDL_RenderCopy(renderer, mapThumbnailTexture, nullptr, &mapThumbnailRect);
            leftArrowButton.draw();
            rightArrowButton.draw();
            selectButton.draw();
            backButton.draw();
            break;
        case GameState::SkinScreen:
            SDL_RenderCopy(renderer, backgroundTextureMapScreen, nullptr, nullptr);
            SDL_RenderCopy(renderer, skinThumbnailTexture, nullptr, &skinThumbnailRect);
            leftArrowButton.draw();
            rightArrowButton.draw();
            selectButton.draw();
            backButton.draw();
            break;
        case GameState::GameScreen:
            SDL_RenderCopy(renderer, backgroundTextureGameScreen, nullptr, nullptr);
            for (const auto &platform : platforms)
            {
                platform.draw(renderer);
            }
            player.update(state, gameState, platforms);
            player.draw(renderer);
            break;
        case GameState::WinScreen:
            SDL_RenderCopy(renderer, backgroundTextureGameScreen, nullptr, nullptr);
            winBackButton.draw();
            break;
        case GameState::LoseScreen:
            SDL_RenderCopy(renderer, backgroundTextureGameScreen, nullptr, nullptr);
            loseBackButton.draw();
            break;
        default:
            break;
        }

        SDL_RenderPresent(renderer);
    }

    SDL_DestroyTexture(backgroundTextureMainMenu);
    SDL_DestroyTexture(backgroundTextureGameScreen);
    SDL_DestroyTexture(backgroundTextureMapScreen);
    SDL_DestroyTexture(playerTexture);
    SDL_DestroyTexture(mapThumbnailTexture);
    SDL_DestroyTexture(skinThumbnailTexture);
    SDL_CloseAudioDevice(audioDevice);
    SDL_FreeWAV(wavStart);
}

int main(int argc, char *args[])
{
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0)
    {
        std::cerr << "SDL could not initialize! SDL Error: " << SDL_GetError() << std::endl;
        return 1;
    }

    if (!(IMG_Init(IMG_INIT_PNG) & IMG_INIT_PNG))
    {
        std::cerr << "SDL_image could not initialize! SDL_image Error: " << IMG_GetError() << std::endl;
        return 1;
    }

    if (TTF_Init() == -1)
    {
        std::cerr << "SDL_ttf could not initialize! SDL_ttf Error: " << TTF_GetError() << std::endl;
        return 1;
    }

    SDL_Window *window = SDL_CreateWindow("Game", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, SCREEN_WIDTH, SCREEN_HEIGHT, SDL_WINDOW_SHOWN);
    if (!window)
    {
        std::cerr << "Window could not be created! SDL Error: " << SDL_GetError() << std::endl;
        return 1;
    }

    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer)
    {
        std::cerr << "Renderer could not be created! SDL Error: " << SDL_GetError() << std::endl;
        return 1;
    }

    runGameLoop(renderer);

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);

    TTF_Quit();
    IMG_Quit();
    SDL_Quit();

    return 0;
}
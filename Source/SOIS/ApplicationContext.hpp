#pragma once

#include <chrono>
#include <vector>

#include "imgui.h"

#include <SDL.h>

#include "glm/glm.hpp"

#include "SOIS/Renderer/Renderer.hpp"

namespace SOIS
{
  // Call only once, loads OpenGL function pointers and other such work.
  void ApplicationInitialization();
  
  using EventHandler = void(*)(SDL_Event&, void*);

  enum PreferredRenderer
  {
    //OpenGL3_3,
	Vulkan,
    //DirectX11,
    DirectX12,
  };

  struct ApplicationContextConfig
  {
    char const* aWindowName = nullptr;
    char const* aIniFile = nullptr;
    EventHandler aHandler = nullptr;
    void* aUserData = nullptr;
    PreferredRenderer aPreferredRenderer;
    bool aBlocking = false;
  };

  struct ApplicationContext
  {
  public:
    SDL_Window* mWindow;
    glm::vec4 mClearColor;

    ApplicationContext(ApplicationContextConfig aConfig);
    ~ApplicationContext();

    // Call when the application should update. Check the return
    // value to see if the application should continue.
    //
    // NOTE: If we're in blocking mode, I would recommend running
    // this in a do ... while loop so that your interface renders
    // in the first frame.
    bool Update();

    // Call when you want the application to end.
    void EndApplication();

    void SetCallbackInfo(EventHandler aHandler, void* aUserData);

    Renderer* GetRenderer()
    {
        return mRenderer.get();
    }

  private:
    void BeginFrame();
    void EndFrame();

    bool ShouldBeBlocking();

    std::vector<SDL_Event> mEvents;
    std::unique_ptr<Renderer> mRenderer;
    EventHandler mHandler;
    void* mUserData;

    std::chrono::time_point<std::chrono::high_resolution_clock> mBegin;
    std::chrono::time_point<std::chrono::high_resolution_clock> mLastFrame;
    double mDt;

    double mTimerUntilBlockingAgain = 0.0f;

    size_t mFrame;
    bool mBlocking;
    bool mRunning;
  };
}
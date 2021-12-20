#pragma once

#include <chrono>
#include <vector>

#include "imgui.h"

#include <SDL.h>

#include "glm/glm.hpp"

#include "SOIS/Renderer.hpp"

namespace SOIS
{
  // Call only once, loads OpenGL function pointers and other such work.
  void ApplicationInitialization();
  
  using EventHandler = void(*)(SDL_Event&, void*);

  enum PreferredRenderer
  {
    OpenGL3_3,
    DirectX11
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

  struct Touch
  {
      // Pressed Down this frame
      bool Pressed()
      {
          return mDown && !mDownPrevious;
      }

      // Let go this frame
      bool Released()
      {
          return !mDown && mDownPrevious;
      }

      bool Held()
      {
          return mDown && mDownPrevious;
      }

      // Only valid when one of the above functions returns true, otherwise
      // undefined. (As of writing 0 or whatever it was last set to)
      // These are defined in WindowCoordinates
      glm::vec2 mFingerPosition = { 0.f, 0.f };
      glm::vec2 mFingerDelta = { 0.f, 0.f };
      bool mDown = false;
      bool mDownPrevious = false;
      
      // Only valid when one of the above functions returns true, otherwise
      // undefined. (As of writing 0 or whatever it was last set to)
      // These are defined in WindowCoordinates
      glm::vec2 mPinchPosition;
      float mPinchDelta = 0.f;
      bool mPinchEvent = false;
  };

  struct Mouse
  {
      glm::vec2 mMouseWheel = {0.f, 0.f};
      bool mScrollHappened = false;
  };

  struct ApplicationContext
  {
  public:
    SDL_Window* mWindow;
    glm::vec4 mClearColor;
    Touch mTouchData;
    Mouse mMouse;

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
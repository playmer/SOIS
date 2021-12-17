#include <thread>
#include <chrono>

#include <SDL_syswm.h>

#include "imgui.h"
#include "imgui_impl_sdl.h"
#include "imgui_freetype.h"

#include "SOIS/ApplicationContext.hpp"

//#include "SOIS/Renderer/DirectX12/DX12Renderer.hpp"
//#include "SOIS/Renderer/Vulkan/"

namespace SOIS
{
  void ApplicationInitialization()
  {
    // Setup SDL
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_GAMECONTROLLER) != 0)
    {
      printf("Error: %s\n", SDL_GetError());
      exit(-1);
    }

    SDL_GameControllerEventState(SDL_ENABLE);
  }

  ApplicationContext::ApplicationContext(ApplicationContextConfig aConfig)
    : mWindow{nullptr}
    , mClearColor{.22f,.22f,.65f,1.0f}
    , mHandler{ aConfig.aHandler }
    , mUserData{ aConfig.aUserData }
    , mFrame{ 0 }
    , mBlocking{ aConfig.aBlocking }
    , mRunning{ true }

  {
    switch (aConfig.aPreferredRenderer)
    {
      case PreferredRenderer::DirectX12: 
        //mRenderer = std::make_unique<DX12Renderer>();
        break;
      //case PreferredRenderer::DirectX11: 
      //default:
      //  mRenderer = std::make_unique<DX11Renderer>();
      //  break;
    }

    if (nullptr == aConfig.aWindowName)
    {
      aConfig.aWindowName = "Dear ImGui SDL2+OpenGL3 Sample Application";
    }

    SDL_WindowFlags window_flags = (SDL_WindowFlags)(mRenderer->GetAdditionalWindowFlags() | 
                                                     SDL_WINDOW_RESIZABLE | 
                                                     SDL_WINDOW_ALLOW_HIGHDPI);
    mWindow = SDL_CreateWindow(aConfig.aWindowName, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1280, 720, window_flags);

    if (nullptr == mWindow)
    {
      return;
    }

    // Setup Dear ImGui binding
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();

    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;       // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;           // Enable Docking
    //io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;         // Enable Multi-Viewport / Platform Windows

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();

    if (aConfig.aIniFile)
    {
      io.IniFilename = aConfig.aIniFile;
    }

    mRenderer->Initialize(mWindow);

    // Setup style
    ImGui::StyleColorsDark();

    mBegin = std::chrono::high_resolution_clock::now();
    mLastFrame = mBegin;

    // See ImGuiFreeType::RasterizationFlags
    io.Fonts->AddFontFromFileTTF("Roboto_Mono/RobotoMono-Medium.ttf", 16.0f);
    io.Fonts->AddFontDefault();
    //unsigned int flags = ImGuiFreeType::NoHinting;
    unsigned int flags = 0;
    ImGuiFreeType::BuildFontAtlas(io.Fonts, flags);

    // We run a begin frame here once, because the update function (meant to 
    // be run as the condition to a while loop.) needs to run both the begin
    // frame work (which it does after checking if it should continue the 
    // frame), and the end frame work (which it does before checking if it
    // should continue running). So we need to prepare for the first end frame.
    BeginFrame();
  }

  ApplicationContext::~ApplicationContext()
  {
    // Cleanup
    mRenderer.reset();

    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();

    SDL_DestroyWindow(mWindow);
    SDL_Quit();
  }
  
  void ApplicationContext::SetCallbackInfo(EventHandler aHandler, void* aUserData)
  {
    mHandler = aHandler;
    mUserData = aUserData;
  }


  void ApplicationContext::EndApplication()
  {
    mRunning = false;
  }

  bool ApplicationContext::Update()
  {
    EndFrame();

    if (false == mRunning)
    {
      return false;
    }

    using namespace std::chrono;
    duration<double> time_span = duration_cast<duration<double>>(high_resolution_clock::now() - mLastFrame);
    mLastFrame = high_resolution_clock::now();
    mDt = time_span.count();

    BeginFrame();

    return true;
  }

  bool ApplicationContext::ShouldBeBlocking()
  {
    // If we're not requested to be blocking, obviously don't block on input.
    if (false == mBlocking)
    {
      return false;
    }

    // If it's the first two frames, we shouldn't block so we can
    // give the user chance to render something.
    if (2 > mFrame)
    {
      return false;
    }

    // If we got an event less than a second or so ago (set in the BeginFrame function)
    // we stay unblocked for a little bit to allow the UI or whatnot to keep updating.
    if (mTimerUntilBlockingAgain > 0.0f)
    {
      mTimerUntilBlockingAgain -= mDt;
      return false;
    }

    // Otherwise, we should be blocking
    return true;
  }

  void ApplicationContext::BeginFrame()
  {
    // Poll and handle events (inputs, window resize, etc.)
    // You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
    // - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application.
    // - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application.
    // Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
    mEvents.clear();

    // If we're blocking on input, we wait for the next event.
    if (ShouldBeBlocking())
    {
      //using namespace std::chrono_literals;
      //std::this_thread::sleep_for(10ms);

      mEvents.clear();
      mEvents.resize(1);
      SDL_WaitEvent(mEvents.data());

      mTimerUntilBlockingAgain = 2.f;
    }

    // Regardless if we're blocking, gather up the rest of the events this frame.
    SDL_Event polledEvent;
    while (SDL_PollEvent(&polledEvent))
    {
      mEvents.emplace_back(polledEvent);
    }

    for (auto& event : mEvents)
    {
      ImGui_ImplSDL2_ProcessEvent(&event);

      if (nullptr != mHandler)
      {
        mHandler(event, mUserData);
      }

      if (event.type == SDL_QUIT)
      {
        mRunning = false;
      }

      if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_CLOSE && event.window.windowID == SDL_GetWindowID(mWindow))
      {
        mRunning = false;
      }

      if (event.type == SDL_WINDOWEVENT)
      {
        auto windowEvent = event.window;

        if (SDL_WINDOWEVENT_RESIZED      == windowEvent.event || 
            SDL_WINDOWEVENT_SIZE_CHANGED == windowEvent.event)
        {
          int width;
          int height;
          SDL_GetWindowSize(mWindow, &width, &height);
          
          mRenderer->ResizeRenderTarget(width, height);
        }
      }
    }

    // Start the Dear ImGui frame
    mRenderer->NewFrame();
    ImGui_ImplSDL2_NewFrame(mWindow);
    ImGui::NewFrame();
    
    mRenderer->ClearRenderTarget(mClearColor);
  }

  void ApplicationContext::EndFrame()
  {
    // Rendering Dear ImGui.
    ImGui::Render();
    mRenderer->RenderImguiData();

    mRenderer->RenderAndPresent();

	ImGuiIO& io = ImGui::GetIO();

	// Update and Render additional Platform Windows
	// (Platform functions may change the current OpenGL context, so we save/restore it to make it easier to paste this code elsewhere.
	//  For this specific demo app we could also call glfwMakeContextCurrent(window) directly)
	if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
	{
		ImGui::UpdatePlatformWindows();
		ImGui::RenderPlatformWindowsDefault();
	}

    ++mFrame;
  }
}
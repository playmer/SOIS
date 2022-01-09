#include <thread>
#include <chrono>

#include <SDL_syswm.h>

#include "imgui.h"
#include "imgui_impl_sdl.h"
#include "imgui_freetype.h"

#include "SOIS/ApplicationContext.hpp"

#if defined __has_include
#  if __has_include (<vulkan/vulkan.h>)
#    define HAS_VULKAN
#  endif
#endif

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
    : mWindow{ nullptr }
    , mClearColor{ .22f,.22f,.65f,1.0f }
    , mHandler{ aConfig.aHandler }
    , mUserData{ aConfig.aUserData }
    , mFrame{ 0 }
    , mBlocking{ aConfig.aBlocking }
    , mRunning{ true }

  {
    switch (aConfig.aPreferredRenderer)
    {
    case PreferredRenderer::OpenGL3_3:    mRenderer = MakeOpenGL3Renderer(); break;
#if defined(HAS_VULKAN)
    case PreferredRenderer::Vulkan:    mRenderer = MakeVulkanRenderer(); break;
#else
    case PreferredRenderer::Vulkan: break;
#endif
#if defined(_WIN32)
    case PreferredRenderer::DirectX11:  mRenderer = MakeDX11Renderer(); break;
    case PreferredRenderer::DirectX12:  mRenderer = MakeDX12Renderer(); break;
#else
    case PreferredRenderer::DirectX11: break;
    case PreferredRenderer::DirectX12: break;
#endif
    }

    // Someone might request a default renderer that doesn't exist on their platform,
    // if this happens a default case won't catch it, so we catch it after the switch.
    if (nullptr == mRenderer)
    {
      mRenderer = MakeOpenGL3Renderer();
    }

    if (nullptr == aConfig.aWindowName)
    {
      aConfig.aWindowName = u8"Dear ImGui SDL2+OpenGL3 Sample Application";
    }

    SDL_WindowFlags window_flags = (SDL_WindowFlags)(mRenderer->GetAdditionalWindowFlags() |
      SDL_WINDOW_RESIZABLE |
      SDL_WINDOW_ALLOW_HIGHDPI);
    mWindow = SDL_CreateWindow((const char*)aConfig.aWindowName, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1280, 720, window_flags);

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
      io.IniFilename = (const char*)aConfig.aIniFile;
    }

    mRenderer->Initialize(mWindow, aConfig.aPreferredGpu);

    // Setup style
    ImGui::StyleColorsDark();

    mBegin = std::chrono::high_resolution_clock::now();
    mLastFrame = mBegin;

    // See ImGuiFreeType::RasterizationFlags
    //io.Fonts->AddFontFromFileTTF("Roboto_Mono/RobotoMono-Medium.ttf", 16.0f);
    //io.Fonts->AddFontDefault();
    //ImWchar ranges[] = { 0xf000, 0xf3ff, 0 };
    ImFontConfig config;
    config.MergeMode = true;
    //io.Fonts->AddFontFromFileTTF("Noto_Sans/NotoSans-Bold.ttf", 16.0f, &config, ranges);
    //io.Fonts->AddFontFromFileTTF("Noto_Sans/NotoSans-BoldItalic.ttf", 16.0f, &config, ranges);
    //io.Fonts->AddFontFromFileTTF("Noto_Sans/NotoSans-Italic.ttf", 16.0f, &config, ranges);

    static const ImWchar ranges[] =
    {
      //0x0000,  0x007F, // Basic Latin
      //0x0080,  0x00FF, // C1 Controls and Latin-1 Supplement
      //0x0100,  0x017F, // Latin Extended-A
      //0x0180,  0x024F, // Latin Extended-B
      0x0250,  0x02AF, // IPA Extensions
      0x02B0,  0x02FF, // Spacing Modifier Letters
      0x0300,  0x036F, // Combining Diacritical Marks
      0x0370,  0x03FF, // Greek/Coptic
      0x0400,  0x04FF, // Cyrillic
      0x0500,  0x052F, // Cyrillic Supplement
      0x0530,  0x058F, // Armenian
      0x0590,  0x05FF, // Hebrew
      0x0600,  0x06FF, // Arabic
      0x0700,  0x074F, // Syriac
      0x0750,  0x077F, // Undefined
      0x0780,  0x07BF, // Thaana
      0x07C0,  0x08FF, // Undefined
      0x0900,  0x097F, // Devanagari
      0x0980,  0x09FF, // Bengali/Assamese
      0x0A00,  0x0A7F, // Gurmukhi
      0x0A80,  0x0AFF, // Gujarati
      0x0B00,  0x0B7F, // Oriya
      0x0B80,  0x0BFF, // Tamil
      0x0C00,  0x0C7F, // Telugu
      0x0C80,  0x0CFF, // Kannada
      0x0D00,  0x0DFF, // Malayalam
      0x0D80,  0x0DFF, // Sinhala
      0x0E00,  0x0E7F, // Thai
      0x0E80,  0x0EFF, // Lao
      0x0F00,  0x0FFF, // Tibetan
      0x1000,  0x109F, // Myanmar
      0x10A0,  0x10FF, // Georgian
      0x1100,  0x11FF, // Hangul Jamo
      0x1200,  0x137F, // Ethiopic
      0x1380,  0x139F, // Undefined
      0x13A0,  0x13FF, // Cherokee
      0x1400,  0x167F, // Unified Canadian Aboriginal Syllabics
      0x1680,  0x169F, // Ogham
      0x16A0,  0x16FF, // Runic
      0x1700,  0x171F, // Tagalog
      0x1720,  0x173F, // Hanunoo
      0x1740,  0x175F, // Buhid
      0x1760,  0x177F, // Tagbanwa
      0x1780,  0x17FF, // Khmer
      0x1800,  0x18AF, // Mongolian
      0x18B0,  0x18FF, // Undefined
      0x1900,  0x194F, // Limbu
      0x1950,  0x197F, // Tai Le
      0x1980,  0x19DF, // Undefined
      0x19E0,  0x19FF, // Khmer Symbols
      0x1A00,  0x1CFF, // Undefined
      0x1D00,  0x1D7F, // Phonetic Extensions
      0x1D80,  0x1DFF, // Undefined
      0x1E00,  0x1EFF, // Latin Extended Additional
      0x1F00,  0x1FFF, // Greek Extended
      0x2000,  0x206F, // General Punctuation
      0x2070,  0x209F, // Superscripts and Subscripts
      0x20A0,  0x20CF, // Currency Symbols
      0x20D0,  0x20FF, // Combining Diacritical Marks for Symbols
      0x2100,  0x214F, // Letterlike Symbols
      0x2150,  0x218F, // Number Forms
      0x2190,  0x21FF, // Arrows
      0x2200,  0x22FF, // Mathematical Operators
      0x2300,  0x23FF, // Miscellaneous Technical
      0x2400,  0x243F, // Control Pictures
      0x2440,  0x245F, // Optical Character Recognition
      0x2460,  0x24FF, // Enclosed Alphanumerics
      0x2500,  0x257F, // Box Drawing
      0x2580,  0x259F, // Block Elements
      0x25A0,  0x25FF, // Geometric Shapes
      0x2600,  0x26FF, // Miscellaneous Symbols
      0x2700,  0x27BF, // Dingbats
      0x27C0,  0x27EF, // Miscellaneous Mathematical Symbols-A
      0x27F0,  0x27FF, // Supplemental Arrows-A
      0x2800,  0x28FF, // Braille Patterns
      0x2900,  0x297F, // Supplemental Arrows-B
      0x2980,  0x29FF, // Miscellaneous Mathematical Symbols-B
      0x2A00,  0x2AFF, // Supplemental Mathematical Operators
      0x2B00,  0x2BFF, // Miscellaneous Symbols and Arrows
      0x2C00,  0x2E7F, // Undefined
      0x2E80,  0x2EFF, // CJK Radicals Supplement
      0x2F00,  0x2FDF, // Kangxi Radicals
      0x2FE0,  0x2FEF, // Undefined
      0x2FF0,  0x2FFF, // Ideographic Description Characters
      0x3000,  0x303F, // CJK Symbols and Punctuation
      0x3040,  0x309F, // Hiragana
      0x30A0,  0x30FF, // Katakana
      0x3100,  0x312F, // Bopomofo
      0x3130,  0x318F, // Hangul Compatibility Jamo
      0x3190,  0x319F, // Kanbun (Kunten)
      0x31A0,  0x31BF, // Bopomofo Extended
      0x31C0,  0x31EF, // Undefined
      0x31F0,  0x31FF, // Katakana Phonetic Extensions
      0x3200,  0x32FF, // Enclosed CJK Letters and Months
      0x3300,  0x33FF, // CJK Compatibility
      0x3400,  0x4DBF, // CJK Unified Ideographs Extension A
      0x4DC0,  0x4DFF, // Yijing Hexagram Symbols
      0x4E00,  0x9FAF, // CJK Unified Ideographs
      0x9FB0,  0x9FFF, // Undefined
      0xA000,  0xA48F, // Yi Syllables
      0xA490,  0xA4CF, // Yi Radicals
      0xA4D0,  0xABFF, // Undefined
      0xAC00,  0xD7AF, // Hangul Syllables
      0xD7B0,  0xD7FF, // Undefined
      0xD800,  0xDBFF, // High Surrogate Area
      0xDC00,  0xDFFF, // Low Surrogate Area
      0xE000,  0xF8FF, // Private Use Area
      0xF900,  0xFAFF, // CJK Compatibility Ideographs
      0xFB00,  0xFB4F, // Alphabetic Presentation Forms
      0xFB50,  0xFDFF, // Arabic Presentation Forms-A
      0xFE00,  0xFE0F, // Variation Selectors
      0xFE10,  0xFE1F, // Undefined
      0xFE20,  0xFE2F, // Combining Half Marks
      0xFE30,  0xFE4F, // CJK Compatibility Forms
      0xFE50,  0xFE6F, // Small Form Variants
      0xFE70,  0xFEFF, // Arabic Presentation Forms-B
      0xFF00,  0xFFEF, // Halfwidth and Fullwidth Forms
      0xFFF0,  0xFFFF, // Specials
      //0x10000, 0x1007F, // Linear B Syllabary
      //0x10080, 0x100FF, // Linear B Ideograms
      //0x10100, 0x1013F, // Aegean Numbers
      //0x10140, 0x102FF, // Undefined
      //0x10300, 0x1032F, // Old Italic
      //0x10330, 0x1034F, // Gothic
      //0x10380, 0x1039F, // Ugaritic
      //0x10400, 0x1044F, // Deseret
      //0x10450, 0x1047F, // Shavian
      //0x10480, 0x104AF, // Osmanya
      //0x104B0, 0x107FF, // Undefined
      //0x10800, 0x1083F, // Cypriot Syllabary
      //0x10840, 0x1CFFF, // Undefined
      //0x1D000, 0x1D0FF, // Byzantine Musical Symbols
      //0x1D100, 0x1D1FF, // Musical Symbols
      //0x1D200, 0x1D2FF, // Undefined
      //0x1D300, 0x1D35F, // Tai Xuan Jing Symbols
      //0x1D360, 0x1D3FF, // Undefined
      //0x1D400, 0x1D7FF, // Mathematical Alphanumeric Symbols
      //0x1D800, 0x1FFFF, // Undefined
      //0x20000, 0x2A6DF, // CJK Unified Ideographs Extension B
      //0x2A6E0, 0x2F7FF, // Undefined
      //0x2F800, 0x2FA1F, // CJK Compatibility Ideographs Supplement
      //0x2FAB0, 0xDFFFF, // Unused
      //0xE0000, 0xE007F, // Tags
      //0xE0080, 0xE00FF, // Unused
      //0xE0100, 0xE01EF, // Variation Selectors Supplement
      //0xE01F0, 0xEFFFF, // Unused
      //0xF0000, 0xFFFFD, // Supplementary Private Use Area-A
      //0xFFFFE, 0xFFFFF, // Unused
      //0x100000, 0x10FFFD, // Supplementary Private Use Area-B
      0,
    };

    io.Fonts->AddFontFromFileTTF("Noto_Sans/NotoSansJP-Regular.otf", 18.0f, nullptr, io.Fonts->GetGlyphRangesJapanese());
    io.Fonts->AddFontFromFileTTF("Noto_Sans/NotoSansKR-Regular.otf", 18.0f, &config, io.Fonts->GetGlyphRangesKorean());
    io.Fonts->AddFontFromFileTTF("Noto_Sans/NotoSansSymbols-Regular.ttf", 18.0f, &config, ranges);
    io.Fonts->AddFontFromFileTTF("Noto_Sans/NotoSansSymbols2-Regular.ttf", 18.0f, &config, ranges);
    //unsigned int flags = ImGuiFreeType::NoHinting;
    unsigned int flags = 0;
    ImGuiFreeType::BuildFontAtlas(io.Fonts, flags);


    SDL_Rect screenRect;
    SDL_GetDisplayBounds(0, &screenRect);

    printf("screenRect: %d %d", screenRect.w, screenRect.h);
    int w, h;
    SDL_GetWindowSize(mWindow, &w, &h);
    printf("WindowSize: %d %d", w, h);

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
    mTouchData.mPinchEvent = false;
    mTouchData.mPinchDelta = 0;
    mTouchData.mFingerDelta = { 0.f, 0.f };
    mTouchData.mDownPrevious = mTouchData.mDown;
    mMouse.mScrollHappened = false;

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

      switch (event.type)
      {
      case SDL_QUIT:
      {
        mRunning = false;
        break;
      }
      case SDL_WINDOWEVENT:
      {
        auto windowEvent = event.window;
        if (windowEvent.event == SDL_WINDOWEVENT_CLOSE && windowEvent.windowID == SDL_GetWindowID(mWindow))
        {
          mRunning = false;
        }
        else if (SDL_WINDOWEVENT_RESIZED == windowEvent.event ||
          SDL_WINDOWEVENT_SIZE_CHANGED == windowEvent.event)
        {
          int width;
          int height;
          SDL_GetWindowSize(mWindow, &width, &height);

          mRenderer->ResizeRenderTarget(width, height);
        }
        break;
      }
      case SDL_MOUSEWHEEL:
      {
        mMouse.mScrollHappened = true;
        mMouse.mMouseWheel = { event.wheel.x , event.wheel.y };
        break;
      }
      case SDL_MULTIGESTURE:
      {
        int w, h;
        SDL_GetWindowSize(mWindow, &w, &h);
        mTouchData.mPinchPosition = glm::vec2{ event.mgesture.x * w, event.mgesture.y * h };
        mTouchData.mPinchDelta = event.mgesture.dDist;
        mTouchData.mPinchEvent = true;
        break;
      }
      case SDL_FINGERMOTION:
      {
        int w, h;
        SDL_GetWindowSize(mWindow, &w, &h);
        auto tempPosition = glm::vec2{ event.tfinger.x * w, event.tfinger.y * h };
        mTouchData.mFingerDelta = tempPosition - mTouchData.mFingerPosition;
        mTouchData.mFingerPosition = tempPosition;
        break;
      }
      case SDL_FINGERDOWN:
      {
        mTouchData.mDown = true;

        int w, h;
        SDL_GetWindowSize(mWindow, &w, &h);
        mTouchData.mFingerPosition = glm::vec2{ event.tfinger.x * w, event.tfinger.y * h };
        break;
      }
      case SDL_FINGERUP:
      {
        mTouchData.mDown = false;

        int w, h;
        SDL_GetWindowSize(mWindow, &w, &h);
        mTouchData.mFingerPosition = glm::vec2{ event.tfinger.x * w, event.tfinger.y * h };
        break;
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
    mRenderer->Upload();
    // Rendering Dear ImGui.
    ImGui::Render();
    mRenderer->RenderImguiData();

    ImGuiIO& io = ImGui::GetIO();

    // Update and Render additional Platform Windows
    // (Platform functions may change the current OpenGL context, so we save/restore it to make it easier to paste this code elsewhere.
    //  For this specific demo app we could also call glfwMakeContextCurrent(window) directly)
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
      ImGui::UpdatePlatformWindows();
      ImGui::RenderPlatformWindowsDefault();
    }

    mRenderer->Present();

    ++mFrame;
  }
}

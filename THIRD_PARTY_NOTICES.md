# Third-party notices

The MIT license in this repository covers its original mod code and documentation. Third-party components retain their own licenses. Killer Frequency and its assets belong to their respective rights holders; owning the game is required. This repository contains no game executable, IL2CPP binary or metadata dump, extracted textures, save files, or game audio.

## MinHook

MinHook 1.3.4 by Tsuda Kageyu and contributors provides native function hooking. Its source is vendored under `vendor/minhook`. See [MinHook license](vendor/minhook/LICENSE.txt), including the HDE notice, and the [upstream project](https://github.com/TsudaKageyu/minhook/tree/v1.3.4).

## Prism

Prism 0.18.3 by ethindp and contributors provides screen-reader speech. Unmodified headers and attribution notices are included under `vendor/prism`; the pinned official Windows x64 binary is downloaded during dependency setup. Prism is MPL-2.0. See [Prism license](vendor/prism/LICENSES/prism/mpl-2.0.txt), [Prism notices](vendor/prism/NOTICE), and [corresponding source for v0.18.3](https://github.com/ethindp/prism/tree/v0.18.3).

Prism includes dependencies with separate licenses; its complete `LICENSES` tree and NOTICE accompany player packages. This includes NVDA controller attribution. We do not modify Prism or those dependencies.

## Optional NVDA controller fallback

Prism is the default speech path. A developer may optionally supply an NVDA controller client DLL using the explicit build parameter documented in the developer guide. A separately supplied controller is not included in the default public package. Its supplier is responsible for providing the appropriate license and corresponding-source information alongside that binary. The local working-checkpoint copy is excluded from Git and public packages.

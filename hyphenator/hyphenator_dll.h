#pragma once

#if (defined(WIN32) || defined(_WIN32))
#ifdef HYPHENATOR_DLL_EXPORTS
#define HYPHENATOR_DLL_API __declspec(dllexport)
#else
#define HYPHENATOR_DLL_API
#endif
#else
#define HYPHENATOR_DLL_API
#endif

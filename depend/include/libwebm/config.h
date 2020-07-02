#pragma once

#ifndef LIBWEBM_CONFIG_H
#define LIBWEBM_CONFIG_H

#define LIBWEBM_DYNAMIC

#ifdef LIBWEBM_DYNAMIC
#ifdef _WINDLL
#define LIBWEBM_DLL_C_API extern "C" __declspec(dllexport)
#define LIBWEBM_DLL_API __declspec(dllexport)
#define LIBWEBM_NO_EXPORT
#else
#define LIBWEBM_DLL_C_API extern "C" __declspec(dllimport)
#define LIBWEBM_DLL_API __declspec(dllimport)
#define LIBWEBM_NO_EXPORT
#endif
#else
#  ifndef EBML_DLL_API
#    ifdef ebml_EXPORTS
        /* We are building this library */
#      define EBML_DLL_API 
#    else
        /* We are using this library */
#      define EBML_DLL_API 
#    endif
#  endif

#  ifndef EBML_NO_EXPORT
#    define EBML_NO_EXPORT 
#  endif
#endif

#endif /* EBML_DLL_API_H */

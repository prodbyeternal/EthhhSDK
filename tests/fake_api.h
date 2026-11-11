#pragma once
#define ETH_CAPI(rettype, name, args) extern "C" __declspec(dllexport) rettype __cdecl name args

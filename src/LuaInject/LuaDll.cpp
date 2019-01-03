/*

Decoda
Copyright (C) 2007-2013 Unknown Worlds Entertainment, Inc. 

This file is part of Decoda.

Decoda is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Decoda is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Decoda.  If not, see <http://www.gnu.org/licenses/>.

*/

#include "LuaDll.h"
#include "Hook.h"
#include "DebugBackend.h"
#include "StdCall.h"
#include "CriticalSection.h"
#include "CriticalSectionLock.h"
#include "DebugHelp.h"

#include <windows.h>
#include <tlhelp32.h>
#include <psapi.h>
#include <windows.h>
#include <malloc.h>
#include <assert.h>
#include <set>
#include <unordered_map>
#include <unordered_set>

// Macro for convenient pointer addition.
// Essentially treats the last two parameters as DWORDs.  The first
// parameter is used to typecast the result to the appropriate pointer type.
#define MAKE_PTR(cast, ptr, addValue ) (cast)( (DWORD)(ptr)+(DWORD)(addValue))

// When this is defined, additional information about what's going on will be
// output for debugging.
//#define VERBOSE
//#define LOG

typedef const char* (__stdcall *lua_Reader_stdcall) (lua_State*, void*, size_t*);
typedef void        (__stdcall *lua_Hook_stdcall)   (lua_State*, lua_Debug*);

typedef lua_State*      (*lua_open_cdecl_t)             (int stacksize);
typedef lua_State*      (*lua_open_500_cdecl_t)         ();
typedef lua_State*      (*lua_newstate_cdecl_t)         (lua_Alloc, void*);
typedef void            (*lua_close_cdecl_t)            (lua_State*);
typedef lua_State*      (*lua_newthread_cdecl_t)        (lua_State*);
typedef int             (*lua_error_cdecl_t)            (lua_State*);
typedef int             (*lua_absindex_cdecl_t)         (lua_State*, int);
typedef int             (*lua_sethook_cdecl_t)          (lua_State*, lua_Hook, int, int);
typedef int             (*lua_gethookmask_cdecl_t)      (lua_State*);
typedef int             (*lua_getinfo_cdecl_t)          (lua_State*, const char*, lua_Debug* ar);
typedef void            (*lua_remove_cdecl_t)           (lua_State*, int);
typedef void            (*lua_settable_cdecl_t)         (lua_State*, int);
typedef void            (*lua_gettable_cdecl_t)         (lua_State*, int);
typedef void            (*lua_setglobal_cdecl_t)        (lua_State*, const char*);
typedef void            (*lua_getglobal_cdecl_t)        (lua_State*, const char*);
typedef void            (*lua_rawget_cdecl_t)           (lua_State *L, int idx);
typedef void            (*lua_rawgeti_cdecl_t)          (lua_State *L, int idx, int n);
typedef void            (*lua_rawset_cdecl_t)           (lua_State *L, int idx);
typedef void            (*lua_pushstring_cdecl_t)       (lua_State*, const char*);
typedef void            (*lua_pushlstring_cdecl_t)      (lua_State*, const char*, size_t);
typedef int             (*lua_type_cdecl_t)             (lua_State*, int);
typedef const char*     (*lua_typename_cdecl_t)         (lua_State*, int);
typedef void            (*lua_settop_cdecl_t)           (lua_State*, int);
typedef const char*     (*lua_getlocal_cdecl_t)         (lua_State*, const lua_Debug*, int);
typedef const char*     (*lua_setlocal_cdecl_t)         (lua_State*, const lua_Debug*, int);
typedef int             (*lua_getstack_cdecl_t)         (lua_State*, int, lua_Debug*);
typedef void            (*lua_insert_cdecl_t)           (lua_State*, int);
typedef void            (*lua_pushnil_cdecl_t)          (lua_State*);
typedef void            (*lua_pushcclosure_cdecl_t)     (lua_State*, lua_CFunction, int);
typedef void            (*lua_pushvalue_cdecl_t)        (lua_State*, int);
typedef void            (*lua_pushinteger_cdecl_t)      (lua_State*, int);
typedef void            (*lua_pushnumber_cdecl_t)       (lua_State*, lua_Number);
typedef const char*     (*lua_tostring_cdecl_t)         (lua_State*, int);
typedef const char*     (*lua_tolstring_cdecl_t)        (lua_State*, int, size_t*);
typedef int             (*lua_toboolean_cdecl_t)        (lua_State*, int);
typedef int             (*lua_tointeger_cdecl_t)        (lua_State*, int);
typedef lua_Integer     (*lua_tointegerx_cdecl_t)       (lua_State*, int, int*);
typedef lua_CFunction   (*lua_tocfunction_cdecl_t)      (lua_State*, int);
typedef lua_Number      (*lua_tonumber_cdecl_t)         (lua_State*, int);
typedef lua_Number      (*lua_tonumberx_cdecl_t)        (lua_State*, int,  int*);
typedef void*           (*lua_touserdata_cdecl_t)       (lua_State*, int);
typedef int             (*lua_gettop_cdecl_t)           (lua_State*);
typedef int             (*lua_load_510_cdecl_t)         (lua_State*, lua_Reader, void*, const char *chunkname);
typedef int             (*lua_load_cdecl_t)             (lua_State*, lua_Reader, void*, const char *chunkname, const char *mode);
typedef void            (*lua_call_cdecl_t)             (lua_State*, int, int);
typedef void            (*lua_callk_cdecl_t)            (lua_State*, int, int, int, lua_CFunction);
typedef int             (*lua_pcall_cdecl_t)            (lua_State*, int, int, int);
typedef int             (*lua_pcallk_cdecl_t)           (lua_State*, int, int, int, int, lua_CFunction);
typedef void            (*lua_newtable_cdecl_t)         (lua_State*);
typedef void            (*lua_createtable_cdecl_t)      (lua_State*, int, int);
typedef int             (*lua_next_cdecl_t)             (lua_State*, int);
typedef int             (*lua_rawequal_cdecl_t)         (lua_State *L, int idx1, int idx2);
typedef int             (*lua_getmetatable_cdecl_t)     (lua_State*, int objindex);
typedef int             (*lua_setmetatable_cdecl_t)     (lua_State*, int objindex);
typedef int             (*luaL_ref_cdecl_t)             (lua_State *L, int t);
typedef void            (*luaL_unref_cdecl_t)           (lua_State *L, int t, int ref);
typedef int             (*luaL_newmetatable_cdecl_t)    (lua_State *L, const char *tname);
typedef int             (*luaL_loadbuffer_cdecl_t)      (lua_State *L, const char *buff, size_t sz, const char *name);
typedef int             (*luaL_loadbufferx_cdecl_t)     (lua_State *L, const char *buff, size_t sz, const char *name, const char* mode);
typedef int             (*luaL_loadfile_cdecl_t)        (lua_State *L, const char *fileName);
typedef int             (*luaL_loadfilex_cdecl_t)       (lua_State *L, const char *fileName, const char* mode);
typedef const lua_WChar* (*lua_towstring_cdecl_t)       (lua_State *L, int index);
typedef int             (*lua_iswstring_cdecl_t)        (lua_State *L, int index);
typedef const char*     (*lua_getupvalue_cdecl_t)       (lua_State *L, int funcindex, int n);
typedef const char*     (*lua_setupvalue_cdecl_t)       (lua_State *L, int funcindex, int n);
typedef void            (*lua_getfenv_cdecl_t)          (lua_State *L, int index);
typedef int             (*lua_setfenv_cdecl_t)          (lua_State *L, int index);
typedef void            (*lua_pushlightuserdata_cdecl_t)(lua_State *L, void *p);
typedef int             (*lua_pushthread_cdecl_t)       (lua_State *L);
typedef void *          (*lua_newuserdata_cdecl_t)      (lua_State *L, size_t size);
typedef lua_State*      (*luaL_newstate_cdecl_t)        ();
typedef int             (*lua_checkstack_cdecl_t)       (lua_State* L, int extra);

typedef HMODULE         (WINAPI *LoadLibraryExW_t)              (LPCWSTR lpFileName, HANDLE hFile, DWORD dwFlags);
typedef ULONG           (WINAPI *LdrLockLoaderLock_t)           (ULONG flags, PULONG disposition, PULONG cookie);
typedef LONG            (WINAPI *LdrUnlockLoaderLock_t)         (ULONG flags, ULONG cookie);


template<typename T> struct LuaAPIPtr;

template <typename Result, typename... Args> struct LuaAPIPtr<Result(Args...)> {
    typedef Result(*CdeclType)(Args...);
    typedef Result(__stdcall *StdcallType)(Args...);
    CdeclType cdeclptr;

#if DECODA_X64
    CdeclType stdcallptr;

    Result CallEither(Args... args)
    {
        if (cdeclptr) {
            return cdeclptr(args...);
        } else {
            return stdcallptr(args...);
        }
    }

    operator bool() const
    {
        return cdeclptr != NULL || stdcallptr != NULL;
    }

    bool GetIsStdCall()
    {
        return stdcallptr != NULL;
    }

    void SetStdcall()
    {
        stdcallptr = reinterpret_cast<StdcallType>(cdeclptr)
        cdeclptr = NULL;
    }

    template<typename T> void SetFunction(LuaAPIPtr<T>& func)
    {
        if (func.cdeclptr != NULL)
        {
            cdeclptr = reinterpret_cast<CdeclType>(func.cdeclptr);
        }
        else
        {
            stdcallptr = reinterpret_cast<CdeclType>(func.stdcallptr);
        }
    }

    void* GetFunction()
    {
        return cdeclptr != NULL ? reinterpret_cast<void*>(cdeclptr) : reinterpret_cast<void*>(stdcallptr);
    }
#else
    Result CallEither(Args... args)
    {
        return cdeclptr(args...);
    }

    operator bool() const
    {
        return cdeclptr != NULL;
    }

    void SetStdcall() {}

    bool GetIsStdCall()
    {
        return false;
    }

    template<typename T2> void SetFunction(LuaAPIPtr<T2>& func)
    {
        cdeclptr = reinterpret_cast<CdeclType>(func.cdeclptr);
    }

    void* GetFunction()
    {
        return cdeclptr;
    }
#endif

    Result operator()(Args... args)
    {
        return CallEither(args...);
    }

    void SetFunction(void* func)
    {
        cdeclptr = reinterpret_cast<CdeclType>(func);
    }

    template<typename T2> bool GetIsStdCallConvention(Args... args, T2& result)
    {
        result = cdeclptr(args...);
        return false;
    }

    bool GetIsStdCallConvention(Args... args)
    {
        cdeclptr(args...);
        return false;
    }

    template<CdeclType func> static Result WorkerCallback(Args..., int api)
    {
        func(Args..., int api);
    }
};

/**
 * Structure that holds pointers to all of the Lua API functions.
 */
struct LuaInterface
{

    int                          version;   // One of 401, 500, 510, 520
    bool                         finishedLoading;

    bool                         stdcall;

    // Use these instead of the LUA_* constants in lua.h. The value of these
    // change depending on the version of Lua we're using.
    int                          registryIndex;
    int                          globalsIndex;
    int                          hookTailCall;
    int                          hookTailRet;


    // cdecl functions.
    LuaAPIPtr<lua_State*  (int stacksize)>                                lua_open_dll;
    LuaAPIPtr<lua_State*  ()>                                             lua_open_500_dll;
    LuaAPIPtr<lua_State*  (lua_Alloc, void*)>                             lua_newstate_dll;
    LuaAPIPtr<void (lua_State*)>                                          lua_close_dll;
    LuaAPIPtr<lua_State*  (lua_State*)>                                   lua_newthread_dll;
    LuaAPIPtr<int (lua_State*)>                                           lua_error_dll;
    LuaAPIPtr<int (lua_State*, int)>                                      lua_absindex_dll;
    LuaAPIPtr<int (lua_State*, lua_Hook, int, int)>                       lua_sethook_dll;
    LuaAPIPtr<int (lua_State*)>                                           lua_gethookmask_dll;
    LuaAPIPtr<int (lua_State*, const char*, lua_Debug* ar)>               lua_getinfo_dll;
    LuaAPIPtr<void (lua_State*, int)>                                     lua_remove_dll;
    LuaAPIPtr<void (lua_State*, int)>                                     lua_settable_dll;
    LuaAPIPtr<void (lua_State*, int)>                                     lua_gettable_dll;
    LuaAPIPtr<void (lua_State*, const char*)>                             lua_setglobal_dll;
    LuaAPIPtr<void (lua_State*, const char*)>                             lua_getglobal_dll;
    LuaAPIPtr<void (lua_State *L, int idx)>                               lua_rawget_dll;
    LuaAPIPtr<void (lua_State *L, int idx, int n)>                        lua_rawgeti_dll;
    LuaAPIPtr<void (lua_State *L, int idx)>                               lua_rawset_dll;
    LuaAPIPtr<void (lua_State*, const char*)>                             lua_pushstring_dll;
    LuaAPIPtr<void (lua_State*, const char*, size_t)>                     lua_pushlstring_dll;
    LuaAPIPtr<int (lua_State*, int)>                                      lua_type_dll;
    LuaAPIPtr<const char*  (lua_State*, int)>                             lua_typename_dll;
    LuaAPIPtr<void (lua_State*, int)>                                     lua_settop_dll;
    LuaAPIPtr<const char*  (lua_State*, const lua_Debug*, int)>           lua_getlocal_dll;
    LuaAPIPtr<const char*  (lua_State*, const lua_Debug*, int)>           lua_setlocal_dll;
    LuaAPIPtr<int (lua_State*, int, lua_Debug*)>                          lua_getstack_dll;
    LuaAPIPtr<void (lua_State*, int)>                                     lua_insert_dll;
    LuaAPIPtr<void (lua_State*)>                                          lua_pushnil_dll;
    LuaAPIPtr<void (lua_State*, lua_CFunction, int)>                      lua_pushcclosure_dll;
    LuaAPIPtr<void (lua_State*, int)>                                     lua_pushvalue_dll;
    LuaAPIPtr<void (lua_State*, int)>                                     lua_pushinteger_dll;
    LuaAPIPtr<void (lua_State*, lua_Number)>                              lua_pushnumber_dll;
    LuaAPIPtr<const char*  (lua_State*, int)>                             lua_tostring_dll;
    LuaAPIPtr<const char*  (lua_State*, int, size_t*)>                    lua_tolstring_dll;
    LuaAPIPtr<int (lua_State*, int)>                                      lua_toboolean_dll;
    LuaAPIPtr<int (lua_State*, int)>                                      lua_tointeger_dll;
    LuaAPIPtr<lua_Integer (lua_State*, int, int*)>                        lua_tointegerx_dll;
    LuaAPIPtr<lua_CFunction (lua_State*, int)>                            lua_tocfunction_dll;
    LuaAPIPtr<lua_Number (lua_State*, int)>                               lua_tonumber_dll;
    LuaAPIPtr<lua_Number (lua_State*, int, int*)>                         lua_tonumberx_dll;
    LuaAPIPtr<void*  (lua_State*, int)>                                   lua_touserdata_dll;
    LuaAPIPtr<int (lua_State*)>                                           lua_gettop_dll;
    LuaAPIPtr<int (lua_State*, lua_Reader, void*, const char*)>           lua_load_510_dll;
    LuaAPIPtr<int (lua_State*, lua_Reader, void*, const char*, const char*)> lua_load_dll;
    LuaAPIPtr<void (lua_State*, int, int)>                                lua_call_dll;
    LuaAPIPtr<void (lua_State*, int, int, int, lua_CFunction)>            lua_callk_dll;
    LuaAPIPtr<int (lua_State*, int, int, int)>                            lua_pcall_dll;
    LuaAPIPtr<int (lua_State*, int, int, int, int, lua_CFunction)>        lua_pcallk_dll;
    LuaAPIPtr<void (lua_State*)>                                          lua_newtable_dll;
    LuaAPIPtr<void (lua_State*, int, int)>                                lua_createtable_dll;
    LuaAPIPtr<int (lua_State*, int)>                                      lua_next_dll;
    LuaAPIPtr<int (lua_State *L, int idx1, int idx2)>                     lua_rawequal_dll;
    LuaAPIPtr<int (lua_State*, int objindex)>                             lua_getmetatable_dll;
    LuaAPIPtr<int (lua_State*, int objindex)>                             lua_setmetatable_dll;
    LuaAPIPtr<int (lua_State *L, int t)>                                  luaL_ref_dll;
    LuaAPIPtr<void (lua_State *L, int t, int ref)>                        luaL_unref_dll;
    LuaAPIPtr<int (lua_State *L, const char *tname)>                      luaL_newmetatable_dll;
    LuaAPIPtr<int (lua_State *L, const char*, size_t, const char*)>  luaL_loadbuffer_dll;
    LuaAPIPtr<int (lua_State *L, const char*, size_t, const char*, const char*)> luaL_loadbufferx_dll;
    LuaAPIPtr<int (lua_State *L, const char *)>                   luaL_loadfile_dll;
    LuaAPIPtr<int (lua_State *L, const char *, const char*)> luaL_loadfilex_dll;
    LuaAPIPtr<const lua_WChar*  (lua_State *L, int)>                lua_towstring_dll;
    LuaAPIPtr<int (lua_State *L, int index)>                              lua_iswstring_dll;
    LuaAPIPtr<const char*  (lua_State *L, int funcindex, int n)>          lua_getupvalue_dll;
    LuaAPIPtr<const char*  (lua_State *L, int funcindex, int n)>          lua_setupvalue_dll;
    LuaAPIPtr<void (lua_State *L, int index)>                             lua_getfenv_dll;
    LuaAPIPtr<int (lua_State *L, int index)>                              lua_setfenv_dll;
    LuaAPIPtr<void (lua_State *L, void *p)>                               lua_pushlightuserdata_dll;
    LuaAPIPtr<int (lua_State *L)>                                         lua_pushthread_dll;
    LuaAPIPtr<void *  (lua_State *L, size_t size)>                        lua_newuserdata_dll;
    LuaAPIPtr<lua_State*  ()>                                             luaL_newstate_dll;
    LuaAPIPtr<int (lua_State* L, int extra)>                              lua_checkstack_dll;

    lua_CFunction                DecodaOutput;
    lua_CFunction                CPCallHandler;
    lua_Hook                     HookHandler;

};


struct CPCallHandlerArgs
{
    lua_CFunction_dll           function;
    void*                       data;
};

#if DECODA_X64
#define INTERCEPT_PROLOG() stdcall = false;
#define INTERCEPT_EPILOG(argsSize) return result;
#define INTERCEPT_EPILOG_NO_RETURN(argsSize) 
#define NAKED 
#else

#define NAKED __declspec(naked) 

/**
 * This macro outputs the prolog code for a naked intercept function. It
 * should be the first code in the function.
 */
#define INTERCEPT_PROLOG()                          \
	__asm                                           \
    {                                               \
        __asm push    ebp                           \
        __asm mov     ebp,            esp           \
        __asm sub     esp,            __LOCAL_SIZE  \
    }

/**
 * This macro outputs the epilog code for a naked intercept function. It
 * should be the last code in the function. argsSize is the number of 
 * bytes for the arguments to the function (not including the the api parameter).
 * The return from the function should be stored in the "result" variable, and
 * the "stdcall" bool variable determines if the function was called using the
 * stdcall or cdecl calling convention.
 */
#define INTERCEPT_EPILOG(argsSize)                  \
    __asm                                           \
    {                                               \
        __asm mov     eax,            result        \
        __asm cmp     stdcall,        0             \
        __asm mov     esp,            ebp           \
        __asm pop     ebp                           \
        __asm jne     stdcall_ret                   \
        __asm ret     4                             \
    __asm stdcall_ret:                              \
        __asm ret     (4 + argsSize)                \
    }

/**
 * This macro outputs the epilog code for a naked intercept function that doesn't
 * have a return value. It should be the last code in the function. argsSize is the
 * number of  bytes for the arguments to the function (not including the the api
 * parameter). The "stdcall" bool variable determines if the function was called using
 * the stdcall or cdecl calling convention.
 */
#define INTERCEPT_EPILOG_NO_RETURN(argsSize)        \
    __asm                                           \
    {                                               \
        __asm cmp     stdcall,        0             \
        __asm mov     esp,            ebp           \
        __asm pop     ebp                           \
        __asm jne     stdcall_ret                   \
        __asm ret     4                             \
    __asm stdcall_ret:                              \
        __asm ret     (4 + argsSize)                \
    }

#endif

LoadLibraryExW_t                LoadLibraryExW_dll      = NULL;
LdrLockLoaderLock_t             LdrLockLoaderLock_dll   = NULL;
LdrUnlockLoaderLock_t           LdrUnlockLoaderLock_dll = NULL;

bool                            g_loadedLuaFunctions = false;
std::set<std::string>           g_loadedModules;
CriticalSection                 g_loadedModulesCriticalSection;

std::vector<LuaInterface>       g_interfaces;
std::unordered_map<void*, void*>  g_hookedFunctionMap;

std::unordered_set<std::string>   g_warnedAboutLua;   // Indivates that we've warned the module contains Lua functions but none were loaded.
std::unordered_set<std::string>   g_warnedAboutPdb;   // Indicates that we've warned about a module having a mismatched PDB.
bool                            g_warnedAboutThreads = false;
bool                            g_warnedAboutJit     = false;

std::string                     g_symbolsDirectory;
static DWORD                    g_disableInterceptIndex = 0;
bool                            g_initializedDebugHelp = false; 

/**
 * Function called after a library has been loaded by the host application.
 * We use this to check for the Lua dll.
 */
void PostLoadLibrary(HMODULE hModule);

/**
 * Data structure passed into the MemoryReader function.
 */
struct Memory
{
    const char* buffer;
    size_t      size;
};

/**
 * lua_Reader function used to read from a memory buffer.
 */
const char* MemoryReader_cdecl(lua_State* L, void* data, size_t* size)
{
    
    Memory* memory = static_cast<Memory*>(data);
    
    if (memory->size > 0)
    {
        *size = memory->size;
        memory->size = 0;
        return memory->buffer;
    }
    else
    {
        return NULL;
    }

}

/**
 * lua_Reader function used to read from a memory buffer.
 */
const char* __stdcall MemoryReader_stdcall(lua_State* L, void* data, size_t* size)
{
    
    Memory* memory = static_cast<Memory*>(data);
    
    if (memory->size > 0)
    {
        *size = memory->size;
        memory->size = 0;
        return memory->buffer;
    }
    else
    {
        return NULL;
    }

}

#pragma auto_inline(off)
int DecodaOutputWorker(int api, lua_State* L)
{

    const char* message = lua_tostring_dll(api, L, 1);
    DebugBackend::Get().Message(message);
    
    return 0;

}
#pragma auto_inline()

NAKED int DecodaOutput(int api, lua_State* L)
{

    int result;
    bool stdcall;

    INTERCEPT_PROLOG()

    result = DecodaOutputWorker(api, L);

    INTERCEPT_EPILOG(4)

}

#pragma auto_inline(off)
int CPCallHandlerWorker(int api, lua_State* L)
{
    CPCallHandlerArgs args = *static_cast<CPCallHandlerArgs*>(lua_touserdata_dll(api, L, 1));

    // Remove the old args and put the new one on the stack.
    lua_pop_dll(api, L, 1);
    lua_pushlightuserdata_dll(api, L, args.data);

    return args.function(api, L);

}
#pragma auto_inline()

NAKED int CPCallHandler(int api, lua_State* L)
{

    int result;
    bool stdcall;
        
    INTERCEPT_PROLOG()
    
    result = CPCallHandlerWorker(api, L);

    INTERCEPT_EPILOG(4)

}

int lua_cpcall_dll(int api, lua_State *L, lua_CFunction_dll func, void *udn)
{

    CPCallHandlerArgs args;

    args.function   = func;
    args.data       = udn;

    // lua_cpcall is deprecated in Lua 5.2, so do as if it didn't exist
    lua_pushcfunction_dll(api, L, g_interfaces[api].CPCallHandler);
    lua_pushlightuserdata_dll( api, L, &args);
    return lua_pcall_dll(api, L, 1, 0, 0);
}

#pragma auto_inline(off)
void HookHandlerWorker(int api, lua_State* L, lua_Debug* ar)
{
    return DebugBackend::Get().HookCallback(api, L, ar);
}
#pragma auto_inline()

NAKED void HookHandler(int api, lua_State* L, lua_Debug* ar)
{

    bool stdcall;

    INTERCEPT_PROLOG()
    
    HookHandlerWorker(api, L, ar);

    INTERCEPT_EPILOG_NO_RETURN(8)

}

void SetHookMode(int api, lua_State* L, HookMode mode)
{

  if(mode == HookMode_None)
  {
      lua_sethook_dll(api, L, NULL, 0, 0);
  }
  else
  {
      int mask;

      switch (mode)
      {
      case HookMode_CallsOnly:
        mask = LUA_MASKCALL;
        break;
      case HookMode_CallsAndReturns:
        mask = LUA_MASKCALL|LUA_MASKRET;
        break;
      case HookMode_Full:
        mask = LUA_MASKCALL|LUA_MASKRET|LUA_MASKLINE;
        break;
      }

      lua_sethook_dll(api, L, g_interfaces[api].HookHandler, mask, 0);
  }

  
}

int lua_gethookmask(int api, lua_State *L)
{
  return g_interfaces[api].lua_gethookmask_dll(L);
}

HookMode GetHookMode(int api, lua_State* L)
{

  int mask = lua_gethookmask(api, L);

  if(mask == 0)
  {
    return HookMode_None;
  }
  else if(mask == (LUA_MASKCALL))
  {
    return HookMode_CallsOnly;
  }
  else if(mask == (LUA_MASKCALL|LUA_MASKRET))
  {
    return HookMode_CallsAndReturns;
  }
  else
  {
    return HookMode_Full;
  }  
}


bool GetIsHookEventRet( int api, int event)
{
    return event == LUA_HOOKRET || event == g_interfaces[api].hookTailRet;
}

bool GetIsHookEventCall( int api, int event)
{
    return event == LUA_HOOKCALL || event == g_interfaces[api].hookTailCall;
}

int GetEvent(int api, const lua_Debug* ar)
{
    switch( g_interfaces[api].version)
    {
        case 520: return ar->ld52.event;
        default: return ar->ld51.event;
    }
}

int GetNups(int api, const lua_Debug* ar)
{
    switch( g_interfaces[api].version)
    {
        case 520: return ar->ld52.nups;
        default: return ar->ld51.nups;
    }
}

int GetCurrentLine(int api, const lua_Debug* ar)
{
    switch( g_interfaces[api].version)
    {
        case 520: return ar->ld52.currentline;
        default: return ar->ld51.currentline;
    }
}

int GetLineDefined(int api, const lua_Debug* ar)
{
    switch( g_interfaces[api].version)
    {
        case 520: return ar->ld52.linedefined;
        default: return ar->ld51.linedefined;
    }
}

int GetLastLineDefined(int api, const lua_Debug* ar)
{
    switch( g_interfaces[api].version)
    {
        case 520: return ar->ld52.lastlinedefined;
        default: return ar->ld51.lastlinedefined;
    }
}

const char* GetSource(int api, const lua_Debug* ar)
{
    switch( g_interfaces[api].version)
    {
        case 520: return ar->ld52.source;
        default: return ar->ld51.source;
    }
}

const char* GetWhat(int api, const lua_Debug* ar)
{
    switch( g_interfaces[api].version)
    {
        case 520: ar->ld52.what;
        default: return ar->ld51.what;
    }
}

const char* GetName(int api, const lua_Debug* ar)
{
    switch( g_interfaces[api].version)
    {
        case 520: return ar->ld52.name;
        default: return ar->ld51.name;
    }
}

const char* GetHookEventName(int api, const lua_Debug* ar)
{
    int event = GetEvent( api, ar);
    const char* eventType = "Unknown";

    if (event == LUA_HOOKLINE)
    {
        eventType = "LUA_HOOKLINE";
    }
    else if (event == LUA_HOOKRET)
    {
        eventType = "LUA_HOOKRET";
    }
    else if (event == g_interfaces[api].hookTailRet)
    {
      eventType = "LUA_HOOKTAILRET";
    }
    else if (event == g_interfaces[api].hookTailCall)
    {
        eventType = "LUA_HOOKTAILCALL";
    }
    else if (event == LUA_HOOKCALL)
    {
        eventType = "LUA_HOOKCALL";
    }
    return eventType;
}

bool lua_pushthread_dll(int api, lua_State *L)
{

    // These structures are taken out of the Lua 5.0 source code.

    union lua_Value_500
    {
        void*         gc;
        void*         p;
        lua_Number    n;
        int           b;
    };

    struct lua_TObject_500
    {
        int             tt;
        lua_Value_500   value;
    };

    struct lua_State_500
    {
        void*               next;
        unsigned char       tt;
        unsigned char       marked;
        lua_TObject_500*    top;
    };

    #pragma pack(1)

    union lua_Value_500_pack1
    {
        void*         gc;
        void*         p;
        lua_Number    n;
        int           b;
    };

    struct lua_TObject_500_pack1
    {
        int                         tt;
        lua_Value_500_pack1         value;
    };

    struct lua_State_500_pack1
    {
        void*                       next;
        unsigned char               tt;
        unsigned char               marked;
        lua_TObject_500_pack1*      top;
    };

    #pragma pack()

    if (g_interfaces[api].lua_pushthread_dll)
    {
        g_interfaces[api].lua_pushthread_dll(L);
        return true;
    }
    else
    {

        // The actual push thread function doesn't exist (probably Lua 5.0), so
        // emulate it. The lua_pushthread function just pushes the state onto the
        // stack and sets the type to LUA_TTHREAD. We use the pushlightuserdata
        // function which basically does the same thing, except we need to modify the
        // type of the object on the top of the stack.

        lua_pushlightuserdata_dll(api, L, L);

        // Check that the thing we think is pointing to the top of the stack actually
        // is so that we don't overwrite something in memory.

        bool success = false;

        // If the structures are laid out differently in the implementation of Lua
        // we might get crashes, so we wrap the access in a try block.

        __try
        {
            lua_State_500* S = reinterpret_cast<lua_State_500*>(L);
            lua_TObject_500* top = S->top - 1;
            if (top->tt == LUA_TLIGHTUSERDATA && top->value.p == L)
            {
                top->tt = LUA_TTHREAD;
                top->value.gc = L;
                success = true;
            }
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
        }

        if (!success)
        {
            // The unpacked version didn't work out right, so try the version with no alignment.
            __try
            {
                lua_State_500_pack1* S = reinterpret_cast<lua_State_500_pack1*>(L);
                lua_TObject_500_pack1* top = S->top - 1;
                if (top->tt == LUA_TLIGHTUSERDATA && top->value.p == L)
                {
                    top->tt = LUA_TTHREAD;
                    top->value.gc = L;
                    success = true;
                }
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
            }
        }

        if (!success)
        {
            lua_pop_dll(api, L, 1);
            if (!g_warnedAboutThreads)
            {
                DebugBackend::Get().Message("Warning 1006: lua_pushthread could not be emulated due to modifications to Lua. Coroutines may be unstable", MessageType_Warning);
                g_warnedAboutThreads = true;
            }
        }

        return success;

    }

}

void* lua_newuserdata_dll(int api, lua_State *L, size_t size)
{
    return g_interfaces[api].lua_newuserdata_dll(L, size);
}

void EnableIntercepts(bool enableIntercepts)
{
    int value = reinterpret_cast<int>(TlsGetValue(g_disableInterceptIndex));
    if (enableIntercepts)
    {
        --value;
    }
    else
    {
        ++value;
    }
    TlsSetValue(g_disableInterceptIndex, reinterpret_cast<LPVOID>(value));
}

bool GetAreInterceptsEnabled()
{
    int value = reinterpret_cast<int>(TlsGetValue(g_disableInterceptIndex));
    return value <= 0;
}

void RegisterDebugLibrary(int api, lua_State* L)
{
    lua_register_dll(api, L, "decoda_output", g_interfaces[api].DecodaOutput);
}

int GetGlobalsIndex(int api)
{
    assert( g_interfaces[api].version >= 500 && g_interfaces[api].version < 520);
    return g_interfaces[api].globalsIndex;
}

int GetRegistryIndex(int api)
{
    return g_interfaces[api].registryIndex;
}

int lua_absindex_dll(int api, lua_State* L, int i)
{
    if (g_interfaces[api].lua_absindex_dll)
{
        return g_interfaces[api].lua_absindex_dll(L, i);
    }

    // Older version of Lua without lua_absindex API, emulate the macro
    if (i > 0 || i <= GetRegistryIndex(api))
    {
        return i;
    }
    else
    {
        return lua_gettop_dll(api, L) + i + 1;
    }
}

int lua_upvalueindex_dll(int api, int i)
{
   if( g_interfaces[api].version >= 520)
    {
        return GetRegistryIndex(api) - i;
    }
    else 
    {
    return GetGlobalsIndex(api) - i;
}
}

void lua_setglobal_dll(int api, lua_State* L, const char* s)
{
    if (g_interfaces[api].lua_setglobal_dll != NULL)
    {
        g_interfaces[api].lua_setglobal_dll(L, s);
    }
    else
    {
        lua_setfield_dll(api, L, GetGlobalsIndex(api), s);
    }
}

void lua_getglobal_dll(int api, lua_State* L, const char* s)
{
    if (g_interfaces[api].lua_getglobal_dll)
    {
        g_interfaces[api].lua_getglobal_dll(L, s);
    }
    else
    {
        lua_getfield_dll(api, L, GetGlobalsIndex(api), s);
    }
}

void lua_rawgetglobal_dll(int api, lua_State* L, const char* s)
{
    lua_pushglobaltable_dll(api, L);
    int glb = lua_gettop_dll(api, L);
    lua_pushstring_dll(api, L, s);
    lua_rawget_dll(api, L, glb);
    lua_remove_dll(api, L, glb);
}

lua_State* lua_newstate_dll(int api, lua_Alloc f, void* ud)
{
  return g_interfaces[api].lua_newstate_dll(f, ud);
}

void lua_close_dll(int api, lua_State* L)
{
    g_interfaces[api].lua_close_dll(L);
}

lua_State* lua_newthread_dll(int api, lua_State* L)
{
    return g_interfaces[api].lua_newthread_dll(L);
}

int lua_error_dll(int api, lua_State* L)
{
  return g_interfaces[api].lua_error_dll(L);
}

int lua_sethook_dll(int api, lua_State* L, lua_Hook f, int mask, int count)
{
    return g_interfaces[api].lua_sethook_dll(L, f, mask, count);
}

int lua_getinfo_dll(int api, lua_State* L, const char* what, lua_Debug* ar)
{
    return g_interfaces[api].lua_getinfo_dll(L, what, ar);
}

void lua_remove_dll(int api, lua_State* L, int index)
{
    g_interfaces[api].lua_remove_dll(L, index);
}

void lua_settable_dll(int api, lua_State* L, int index)
{
    g_interfaces[api].lua_settable_dll(L, index);
}

void lua_gettable_dll(int api, lua_State* L, int index)
{
    g_interfaces[api].lua_gettable_dll(L, index);
}

void lua_rawget_dll(int api, lua_State* L, int idx)
{
    g_interfaces[api].lua_rawget_dll(L, idx);
}

void lua_rawgeti_dll(int api, lua_State *L, int idx, int n)
{

    g_interfaces[api].lua_rawgeti_dll(L, idx, n);
}

void lua_rawset_dll(int api, lua_State* L, int idx)
{
    g_interfaces[api].lua_rawset_dll(L, idx);
}

void lua_pushstring_dll(int api, lua_State* L, const char* s)
{
    g_interfaces[api].lua_pushstring_dll(L, s);
}

void lua_pushlstring_dll(int api, lua_State* L, const char* s, size_t len)
{
    g_interfaces[api].lua_pushlstring_dll(L, s, len);
}

int lua_type_dll(int api, lua_State* L, int index)
{
    return g_interfaces[api].lua_type_dll(L, index);
}

const char* lua_typename_dll(int api, lua_State* L, int type)
{
    return g_interfaces[api].lua_typename_dll(L, type);
}

int lua_checkstack_dll(int api, lua_State* L, int extra)
{
    return g_interfaces[api].lua_checkstack_dll(L, extra);
}

void lua_getfield_dll(int api, lua_State* L, int index, const char* k)
{

    // Since Lua 4.0 doesn't include lua_getfield, we just emulate its
    // behavior for simplicity.

    index = lua_absindex_dll(api, L, index);

    lua_pushstring_dll(api, L, k);
    lua_gettable_dll(api, L, index);

}

void lua_setfield_dll(int api, lua_State* L, int index, const char* k)
{

    // Since Lua 4.0 doesn't include lua_setfield, we just emulate its
    // behavior for simplicity.

    index = lua_absindex_dll(api, L, index);

    lua_pushstring_dll(api, L, k);
    lua_insert_dll(api, L, -2);
    lua_settable_dll(api, L, index);

}

void lua_settop_dll(int api, lua_State* L, int index)
{
    g_interfaces[api].lua_settop_dll(L, index);
}

const char* lua_getlocal_dll(int api, lua_State* L, const lua_Debug* ar, int n)
{
    return g_interfaces[api].lua_getlocal_dll(L, ar, n);
}

const char* lua_setlocal_dll(int api, lua_State* L, const lua_Debug* ar, int n)
{

    return g_interfaces[api].lua_setlocal_dll(L, ar, n);
}

int lua_getstack_dll(int api, lua_State* L, int level, lua_Debug* ar)
{
    return g_interfaces[api].lua_getstack_dll(L, level, ar);
}

void lua_insert_dll(int api, lua_State* L, int index)
{
    g_interfaces[api].lua_insert_dll(L, index);
}

void lua_pushnil_dll(int api, lua_State* L)
{
    g_interfaces[api].lua_pushnil_dll(L);
}

void lua_pushcclosure_dll(int api, lua_State* L, lua_CFunction fn, int n)
{
    g_interfaces[api].lua_pushcclosure_dll(L, fn, n);
}

void lua_pushvalue_dll(int api, lua_State* L, int index)
{
    g_interfaces[api].lua_pushvalue_dll(L, index);
}

void lua_pushnumber_dll(int api, lua_State* L, lua_Number value)
{
    g_interfaces[api].lua_pushnumber_dll(L, value);
}

void lua_pushinteger_dll(int api, lua_State* L, int value)
{
    if (g_interfaces[api].lua_pushinteger_dll) 
    {
        // Lua 5.0 version.
        return g_interfaces[api].lua_pushinteger_dll(L, value);
    }
    else
    {
        // Fallback to lua_pushnumber on Lua 4.0.
        lua_pushnumber_dll(api, L, static_cast<lua_Number>(value));
    }
}

void lua_pushlightuserdata_dll(int api, lua_State* L, void* p)
{
    g_interfaces[api].lua_pushlightuserdata_dll(L, p);
}

void lua_pushglobaltable_dll(int api, lua_State* L)
{
    if( g_interfaces[api].version >= 520)
    {
        lua_rawgeti_dll( api, L, GetRegistryIndex(api), g_interfaces[api].globalsIndex);
    }
    else
    {
        lua_pushvalue_dll( api, L, GetGlobalsIndex(api));
    }
}

const char* lua_tostring_dll(int api, lua_State* L, int index)
{
    return g_interfaces[api].lua_tolstring_dll(L, index, NULL);
}

const char* lua_tolstring_dll(int api, lua_State* L, int index, size_t* len)
{
    return g_interfaces[api].lua_tolstring_dll(L, index, len);
}

int lua_toboolean_dll(int api, lua_State* L, int index)
{
    return g_interfaces[api].lua_toboolean_dll(L, index);
}

int lua_tointeger_dll(int api, lua_State* L, int index)
{
    if (g_interfaces[api].lua_tointegerx_dll) {
        // Lua 5.2 implementation.
        return (int)g_interfaces[api].lua_tointegerx_dll(L, index, NULL);
    }
    else 
    {
        // Lua 5.0 implementation.
        return g_interfaces[api].lua_tointeger_dll(L, index);
    }
}

lua_CFunction lua_tocfunction_dll(int api, lua_State* L, int index)
{
    return g_interfaces[api].lua_tocfunction_dll(L, index);
}

lua_Number lua_tonumber_dll(int api, lua_State* L, int index)
{
    if (g_interfaces[api].lua_tonumberx_dll)
    {
        // Lua 5.2 implementation.
        return g_interfaces[api].lua_tonumberx_dll(L, index, NULL);
    }
    else 
    {
        return g_interfaces[api].lua_tonumber_dll(L, index);
    }
}

void* lua_touserdata_dll(int api, lua_State *L, int index)
{
    return g_interfaces[api].lua_touserdata_dll(L, index);
}

int lua_gettop_dll(int api, lua_State* L)
{
    return g_interfaces[api].lua_gettop_dll(L);
}

int lua_loadbuffer_dll(int api, lua_State* L, const char* buffer, size_t size, const char* chunkname, const char* mode)
{

    Memory memory;

    memory.buffer   = buffer;
    memory.size     = size;

    if (g_interfaces[api].lua_load_dll)
    {
        return g_interfaces[api].lua_load_dll(L, MemoryReader_cdecl, &memory, chunkname, mode);
    }
    else if (g_interfaces[api].lua_load_510_dll)
    {
        return g_interfaces[api].lua_load_510_dll(L, MemoryReader_cdecl, &memory, chunkname);
    }

    assert(0);
    return 0;
}

void lua_call_dll(int api, lua_State* L, int nargs, int nresults)
{
    return g_interfaces[api].lua_call_dll(L, nargs, nresults);
}

int lua_pcallk_dll(int api, lua_State* L, int nargs, int nresults, int errfunc, int ctx, lua_CFunction k)
{
    return g_interfaces[api].lua_pcallk_dll(L, nargs, nresults, errfunc, ctx, k);
}

int lua_pcall_dll(int api, lua_State* L, int nargs, int nresults, int errfunc)
{
    // Lua 5.2.
    if (g_interfaces[api].lua_pcallk_dll)
    {
        return lua_pcallk_dll(api, L, nargs, nresults, errfunc, 0, NULL);
    } 
    else
    {
        // Lua 5.1 and earlier.
        return g_interfaces[api].lua_pcall_dll(L, nargs, nresults, errfunc);
    }
}

void lua_newtable_dll(int api, lua_State* L)
{
    g_interfaces[api].lua_createtable_dll(L, 0, 0);
}

int lua_next_dll(int api, lua_State* L, int index)
{
    return g_interfaces[api].lua_next_dll(L, index);
}

int lua_rawequal_dll(int api, lua_State *L, int idx1, int idx2)
{
    return g_interfaces[api].lua_rawequal_dll(L, idx1, idx2);
}

int lua_getmetatable_dll(int api, lua_State* L, int index)
{
    return g_interfaces[api].lua_getmetatable_dll(L, index);
}

int lua_setmetatable_dll(int api, lua_State* L, int index)
{
    return g_interfaces[api].lua_setmetatable_dll(L, index);
}

int luaL_ref_dll(int api, lua_State *L, int t)
{
    if (g_interfaces[api].luaL_ref_dll)
    {
        return g_interfaces[api].luaL_ref_dll(L, t);
    }
    // We don't require that luaL_ref be present, so provide a suitable
    // implementation if it's not.
    return LUA_NOREF;
}

void luaL_unref_dll(int api, lua_State *L, int t, int ref)
{
    g_interfaces[api].luaL_unref_dll(L, t, ref);
}

int luaL_newmetatable_dll(int api, lua_State *L, const char *tname)
{
    return g_interfaces[api].luaL_newmetatable_dll(L, tname);
}

int luaL_loadbuffer_dll(int api, lua_State *L, const char *buff, size_t sz, const char *name)
{
        return g_interfaces[api].luaL_loadbuffer_dll(L, buff, sz, name);
}

int luaL_loadbufferx_dll(int api, lua_State *L, const char *buff, size_t sz, const char *name, const char* mode)
{
        return g_interfaces[api].luaL_loadbufferx_dll(L, buff, sz, name, mode);
}

int luaL_loadfile_dll(int api, lua_State* L, const char* fileName)
{
        return g_interfaces[api].luaL_loadfile_dll(L, fileName);
}

int luaL_loadfilex_dll(int api, lua_State* L, const char* fileName, const char* mode)
{
        return g_interfaces[api].luaL_loadfilex_dll(L, fileName, mode);
}

lua_State* luaL_newstate_dll(int api)
{
        return g_interfaces[api].luaL_newstate_dll();
}

const lua_WChar* lua_towstring_dll(int api, lua_State* L, int index)
{
    if (g_interfaces[api].lua_towstring_dll)
    {
        return g_interfaces[api].lua_towstring_dll(L, index);
    }
    else
    {
        // The application is not using LuaPlus, so just return NULL.
        return NULL;
    }
}

int lua_iswstring_dll(int api, lua_State* L, int index)
{
    if (g_interfaces[api].lua_iswstring_dll)
    {
        return g_interfaces[api].lua_iswstring_dll(L, index);
    }
    else
    {
        // The application is not using LuaPlus, so just return 0.
        return 0;
    }
}

const char* lua_getupvalue_dll(int api, lua_State *L, int funcindex, int n)
{
    return g_interfaces[api].lua_getupvalue_dll(L, funcindex, n);
}

const char* lua_setupvalue_dll(int api, lua_State *L, int funcindex, int n)
{
    return g_interfaces[api].lua_setupvalue_dll(L, funcindex, n);
}

void lua_getfenv_dll(int api, lua_State *L, int index)
{
    if (g_interfaces[api].lua_getfenv_dll)
    {
        g_interfaces[api].lua_getfenv_dll(L, index);
    }
    else // no lua_setfenv: Lua 5.2+ uses an upvalue named _ENV instead
    {
        index = lua_absindex_dll( api, L, index);
        // look for it
        int upidx = 1;
        const char* upname = NULL;
        while ((upname = lua_getupvalue_dll( api, L, index, upidx)) != NULL && strcmp( upname, "_ENV") != 0)
        {
            lua_pop_dll( api, L, 1);
            ++ upidx;
        }
        // _ENV is left on the stack (or not)
        if (upname == NULL)
        {
            lua_pushnil_dll( api, L);
        }
    }
}

int lua_setfenv_dll(int api, lua_State *L, int index)
{
    if (g_interfaces[api].lua_setfenv_dll)
    {
        return g_interfaces[api].lua_setfenv_dll(L, index);
    }
    else // no lua_setfenv: Lua 5.2+ uses an upvalue named _ENV instead
    {
        index = lua_absindex_dll( api, L, index);
        // look for it
        int upidx = 1;
        const char* upname = NULL;
        while( (upname = lua_getupvalue_dll( api, L, index, upidx)) != NULL && strcmp( upname, "_ENV") != 0)
    {
            lua_pop_dll( api, L, 1);
            ++ upidx;
    }
        if (upname != NULL)
    {
            lua_pop_dll( api, L, 1); // pop the actual value, we are only interested in its index
            lua_setupvalue_dll( api, L, index, upidx);
            return 1;
        }
        // "If the value at the given index is neither a function nor a thread nor a userdata, lua_setfenv returns 0."
        lua_pop_dll( api, L, 1);
        return 0;
    }
}

HMODULE WINAPI LoadLibraryExW_intercept(LPCWSTR fileName, HANDLE hFile, DWORD dwFlags)
{

    // We have to call the loader lock (if it is available) so that we don't get deadlocks
    // in the case where Dll initialization acquires the loader lock and calls LoadLibrary
    // while another thread is inside PostLoadLibrary.

    ULONG cookie;

    if (LdrLockLoaderLock_dll   != NULL &&
        LdrUnlockLoaderLock_dll != NULL)
    {
        LdrLockLoaderLock_dll(0, 0, &cookie);
    }

    HMODULE hModule = LoadLibraryExW_dll(fileName, hFile, dwFlags);

    if (hModule != NULL)
    {
        PostLoadLibrary(hModule);
    }

    if (LdrLockLoaderLock_dll   != NULL &&
        LdrUnlockLoaderLock_dll != NULL)
    {
        LdrUnlockLoaderLock_dll(0, cookie);
    }

    return hModule;

}

void FinishLoadingLua(int api)
{

    if (g_interfaces[api].finishedLoading)
    {
        return;
    }

    g_interfaces[api].finishedLoading = true;

    DebugBackend::Get().CreateApi(api);

}

#pragma auto_inline(off)
void lua_call_worker(int api, lua_State* L, int nargs, int nresults)
{

    if (!g_interfaces[api].finishedLoading)
    {
        g_interfaces[api].lua_call_dll(L, nargs, nresults);
        FinishLoadingLua(api);
        DebugBackend::Get().AttachState(api, L);
    
    }
    else
    {

        DebugBackend::Get().AttachState(api, L);

        if (lua_gettop_dll(api, L) < nargs + 1)
        {
            DebugBackend::Get().Message("Warning 1005: lua_call called with too few arguments on the stack", MessageType_Warning);
        }

        if (DebugBackend::Get().Call(api, L, nargs, nresults, 0))
        {
            lua_error_dll(api, L);
        }

    }

}
#pragma auto_inline()

void lua_callk_worker(int api, lua_State* L, int nargs, int nresults, int ctk, lua_CFunction k)
{

    if (!g_interfaces[api].finishedLoading)
    {
        g_interfaces[api].lua_callk_dll(L, nargs, nresults, ctk, k);

        FinishLoadingLua(api);
        DebugBackend::Get().AttachState(api, L);
    
    }
    else
    {

        DebugBackend::Get().AttachState(api, L);

        if (lua_gettop_dll(api, L) < nargs + 1)
        {
            DebugBackend::Get().Message("Warning 1005: lua_call called with too few arguments on the stack", MessageType_Warning);
        }

        if (DebugBackend::Get().Call(api, L, nargs, nresults, 0))
        {
            lua_error_dll(api, L);
        }

    }

}
#pragma auto_inline()


#pragma auto_inline(off)
int lua_pcall_worker(int api, lua_State* L, int nargs, int nresults, int errfunc)
{

    int result;

    if (!g_interfaces[api].finishedLoading)
    {
        result = g_interfaces[api].lua_pcall_dll(L, nargs, nresults, errfunc);
        FinishLoadingLua(api);
        DebugBackend::Get().AttachState(api, L);
    
    }
    else
    {

        DebugBackend::Get().AttachState(api, L);

        if (lua_gettop_dll(api, L) < nargs + 1)
        {
            DebugBackend::Get().Message("Warning 1005: lua_pcall called with too few arguments on the stack", MessageType_Warning);
        }

        if (GetAreInterceptsEnabled())
        {
            result = DebugBackend::Get().Call(api, L, nargs, nresults, errfunc);
        }
        else
        {
            result = lua_pcall_dll(api, L, nargs, nresults, errfunc);
        }

    }

    return result;    

}
#pragma auto_inline()


#pragma auto_inline(off)
int lua_pcallk_worker(int api, lua_State* L, int nargs, int nresults, int errfunc, int ctx, lua_CFunction k)
{

    int result;

    if (!g_interfaces[api].finishedLoading)
    {
        result = g_interfaces[api].lua_pcall_dll(L, nargs, nresults, errfunc);

        FinishLoadingLua(api);
        DebugBackend::Get().AttachState(api, L);
    
    }
    else
    {

        DebugBackend::Get().AttachState(api, L);

        if (lua_gettop_dll(api, L) < nargs + 1)
        {
            DebugBackend::Get().Message("Warning 1005: lua_pcallk called with too few arguments on the stack", MessageType_Warning);
        }

        if (GetAreInterceptsEnabled())
        {
            result = DebugBackend::Get().Call(api, L, nargs, nresults, errfunc);
        }
        else
        {
            result = lua_pcallk_dll(api, L, nargs, nresults, errfunc, ctx, k);
        }

    }

    return result;    

}
#pragma auto_inline()

#pragma auto_inline(off)
lua_State* lua_newstate_worker(int api, lua_Alloc f, void* ud)
{

    lua_State* result = NULL;

    if (!g_interfaces[api].finishedLoading)
    {
        result = g_interfaces[api].lua_newstate_dll(f, ud);
        FinishLoadingLua(api);
    }
    else if (g_interfaces[api].lua_newstate_dll)
    {
        result = g_interfaces[api].lua_newstate_dll(f, ud);
    }
    
    if (result != NULL)
    {
        DebugBackend::Get().AttachState(api, result);
    }

    return result;

}
#pragma auto_inline()

#pragma auto_inline(off)
lua_State* lua_newthread_worker(int api, lua_State* L)
{
    
    lua_State* result = NULL;

    if (!g_interfaces[api].finishedLoading)
    {
        result = g_interfaces[api].lua_newthread_dll(L);
        FinishLoadingLua(api);
    }
    else if (g_interfaces[api].lua_newthread_dll)
    {
        result = g_interfaces[api].lua_newthread_dll(L);
    }
    
    if (result != NULL)
    {
        DebugBackend::Get().AttachState(api, result);
    }

    return result;

}
#pragma auto_inline()

#pragma auto_inline(off)
lua_State* lua_open_worker(int api, int stacksize)
{

    lua_State* result = NULL;

    if (!g_interfaces[api].finishedLoading)
    {
        result = g_interfaces[api].lua_open_dll(stacksize);
        FinishLoadingLua(api);
    }
    else if (g_interfaces[api].lua_open_dll)
    {
        result = g_interfaces[api].lua_open_dll(stacksize);
    }
    
    if (result != NULL)
    {
        DebugBackend::Get().AttachState(api, result);
    }

    return result;

}
#pragma auto_inline()

#pragma auto_inline(off)
lua_State* lua_open_500_worker(int api)
{

    lua_State* result = NULL;

    if (!g_interfaces[api].finishedLoading)
    {
        FinishLoadingLua(api);
    }

    if (g_interfaces[api].lua_open_500_dll)
    {
        result = g_interfaces[api].lua_open_500_dll();
    }
    
    if (result != NULL)
    {
        DebugBackend::Get().AttachState(api, result);
    }

    return result;

}
#pragma auto_inline()

#pragma auto_inline(off)
int lua_load_worker(int api, lua_State* L, lua_Reader reader, void* data, const char* name, const char* mode)
{
    // Read all of the data out of the reader and into a big buffer.

    std::vector<char> buffer;

    const char* chunk;
    size_t chunkSize;

    do
    {

        if (!g_interfaces[api].finishedLoading)
        {
            // In this case we must have attached the debugger so we're intercepting a lua_load
            // function before we've initialized.
            chunk = reader(L, data, &chunkSize);

            FinishLoadingLua(api);
        }
        else
        {
            // We assume that since the lua_load function is cdecl the reader function is as well.
            chunk = reader(L, data, &chunkSize);
        }

        // We allow the reader to return 0 for the chunk size since Lua supports
        // that, although according to the manual it should return NULL to signal
        // the end of the data.

        if (chunk != NULL && chunkSize > 0)
        {
            buffer.insert(buffer.end(), chunk, chunk + chunkSize);
        }

    }
    while (chunk != NULL && chunkSize > 0);

    const char* source = NULL;
    
    if (!buffer.empty())
    {
        source = &buffer[0];
    }

    // Make sure the debugger knows about this state. This is necessary since we might have
    // attached the debugger after the state was created.
    DebugBackend::Get().AttachState(api, L);

    // Disables JIT compilation if LuaJIT is being used. Otherwise we won't get hooks for
    // this chunk.
    if (DebugBackend::Get().EnableJit(api, L, false))
    {
        if (!g_warnedAboutJit)
        {
            DebugBackend::Get().Message("Warning 1007: Just-in-time compilation of Lua code disabled to allow debugging", MessageType_Warning);
            g_warnedAboutJit = true;
        }
    }

    int result = lua_loadbuffer_dll(api, L, source, buffer.size(), name, mode);

    if (!buffer.empty())
    {
        result = DebugBackend::Get().PostLoadScript(api, result, L, source, buffer.size(), name);
    }

    return result;
   
}
#pragma auto_inline()

#pragma auto_inline(off)
void lua_close_worker(int api, lua_State* L)
{
    
    if (!g_interfaces[api].finishedLoading)
    {
        FinishLoadingLua(api);
    }
    else if (g_interfaces[api].lua_close_dll)
    {
        g_interfaces[api].lua_close_dll(L);
    }

    DebugBackend::Get().DetachState(api, L);
    
}
#pragma auto_inline()

#pragma auto_inline(off)
int luaL_newmetatable_worker(int api, lua_State *L, const char* tname)
{

    int result;

    if (!g_interfaces[api].finishedLoading)
    {
        FinishLoadingLua(api);
    }
    else if (g_interfaces[api].luaL_newmetatable_dll)
    {
        result = g_interfaces[api].luaL_newmetatable_dll(L, tname);
    }

    if (result != 0)
    {
        // Only register if we haven't seen this name before.
        DebugBackend::Get().RegisterClassName(api, L, tname, lua_gettop_dll(api, L));
    }

    return result;

}
#pragma auto_inline()

#pragma auto_inline(off)
int lua_sethook_worker(int api, lua_State *L, lua_Hook f, int mask, int count)
{
    
    // Currently we're using the hook and can't let anyone else use it.
    // What we should do is implement the lua hook on top of our existing hook.
        
    int result = 0;
 
    if (!g_interfaces[api].finishedLoading)
    {
        result = g_interfaces[api].lua_sethook_dll(L, f, mask, count);
        FinishLoadingLua(api);
        DebugBackend::Get().AttachState(api, L);
    }
    else
    {
        // Note, the lua_hook call is currently bypassed.
    }

    return result;

}
#pragma auto_inline()

#pragma auto_inline(off)
int luaL_loadbufferx_worker(int api, lua_State *L, const char *buff, size_t sz, const char *name, const char* mode)
{

    int result = 0;

    if (!g_interfaces[api].finishedLoading)
    {
        result = g_interfaces[api].luaL_loadbuffer_dll(L, buff, sz, name);
        FinishLoadingLua(api);
    }
    else if (g_interfaces[api].luaL_loadbufferx_dll)
    {
        result = g_interfaces[api].luaL_loadbufferx_dll(L, buff, sz, name, mode);
    }
    else if (g_interfaces[api].luaL_loadbuffer_dll)
    {
        result = g_interfaces[api].luaL_loadbuffer_dll(L, buff, sz, name);
    }

    // Make sure the debugger knows about this state. This is necessary since we might have
    // attached the debugger after the state was created.
    DebugBackend::Get().AttachState(api, L);

    return DebugBackend::Get().PostLoadScript(api, result, L, buff, sz, name);

}
#pragma auto_inline()

#pragma auto_inline(off)
int luaL_loadfilex_worker(int api, lua_State *L, const char *fileName, const char* mode)
{

    int result = 0;

    if (!g_interfaces[api].finishedLoading)
    {
        result = g_interfaces[api].luaL_loadfile_dll(L, fileName);
        FinishLoadingLua(api);
    }
    else if (g_interfaces[api].luaL_loadfilex_dll)
    {
        result = g_interfaces[api].luaL_loadfilex_dll(L, fileName, mode);
    }
    else if (g_interfaces[api].luaL_loadfile_dll)
    {
        result = g_interfaces[api].luaL_loadfile_dll(L, fileName);
    }

    // Make sure the debugger knows about this state. This is necessary since we might have
    // attached the debugger after the state was created.
    DebugBackend::Get().AttachState(api, L);

    // Load the file.

    FILE* file = fopen(fileName, "rb");

    if (file != NULL)
    {

        std::string name = "@";
        name += fileName;

        fseek(file, 0, SEEK_END);
        unsigned int length = ftell(file);

        char* buffer = new char[length];
        fseek(file, 0, SEEK_SET);
        fread(buffer, 1, length, file);

        fclose(file);

        result = DebugBackend::Get().PostLoadScript(api, result, L, buffer, length, name.c_str());

        delete [] buffer;
    
    }

    return result;

}
#pragma auto_inline()

#pragma auto_inline(off)
lua_State* luaL_newstate_worker(int api)
{

    lua_State* result = g_interfaces[api].luaL_newstate_dll();

    // Since we couldn't test if luaL_newstate was stdcall or cdecl (since it
    // doesn't have any arguments), call another function. lua_gettop is a good
    // choice since it has no side effects.

    if (!g_interfaces[api].finishedLoading && result != NULL)
    {
        FinishLoadingLua(api);
    }
    
    if (result != NULL)
    {
        DebugBackend::Get().AttachState(api, result);
    }

    return result;

}
#pragma auto_inline()

std::string GetEnvironmentVariable(const std::string& name)
{

    DWORD size = ::GetEnvironmentVariable(name.c_str(), NULL, 0);

    std::string result;

    if (size > 0)
    {
    
        char* buffer = new char[size];
        buffer[0] = 0;

        GetEnvironmentVariable(name.c_str(), buffer, size);

        result = buffer;
        delete [] buffer;

    }

    return result;

}

std::string GetApplicationDirectory()
{

    char fileName[_MAX_PATH];
    GetModuleFileNameEx(GetCurrentProcess(), NULL, fileName, _MAX_PATH);

    char* term = strrchr(fileName, '\\'); 

    if (term != NULL)
    {
        *term = 0;
    }

    return fileName;

}

static lua_State* luaL_newstate_intercept()
{
    int api = 0;
    return  luaL_newstate_worker(api);
}

static lua_State* lua_newstate_intercept(lua_Alloc f, void* ud)
{
    int api = 0;
    return lua_newstate_worker(api, f, ud);
}

static lua_State* lua_newthread_intercept(lua_State* L)
{
    int api = 0;
    return lua_newthread_worker(api, L);
}

static lua_State* lua_open_intercept(int stacksize)
{
    int api = 0;
    return lua_open_worker(api, stacksize);
}

static void lua_call_intercept(lua_State* L, int nargs, int nresults)
{
    int api = 0;
    lua_call_worker(api, L, nargs, nresults);
}

static void lua_callk_intercept(lua_State* L, int nargs, int nresults, int ctx, lua_CFunction k)
{
    int api = 0;
    lua_callk_worker(api, L, nargs, nresults, ctx, k);
}

static int lua_pcall_intercept(lua_State* L, int nargs, int nresults, int errfunc)
{
    int api = 0;
    return lua_pcall_worker(api, L, nargs, nresults, errfunc);
}


static int lua_pcallk_intercept(lua_State* L, int nargs, int nresults, int errfunc, int ctx, lua_CFunction k)
{
    int api = 0;
    return lua_pcallk_worker(api, L, nargs, nresults, errfunc, ctx, k);
}

static int lua_load_510_intercept(lua_State* L, lua_Reader reader, void* data, const char* name)
{
    int api = 0;
    return lua_load_worker(api, L, reader, data, name, NULL);
}

static int lua_load_intercept(lua_State* L, lua_Reader reader, void* data, const char* name, const char* mode)
{
    int api = 0;
    return lua_load_worker(api, L, reader, data, name, mode);
}

static void lua_close_intercept(lua_State* L)
{
    int api = 0;
    lua_close_worker(api, L);
}

static int luaL_newmetatable_intercept(lua_State* L, const char* tname)
{
    int api = 0;
    return luaL_newmetatable_worker(api, L, tname);
}

static int lua_sethook_intercept(lua_State *L, lua_Hook f, int mask, int count)
{
    int api = 0;
    return lua_sethook_worker(api, L, f, mask, count);
}

static int luaL_loadbuffer_intercept(lua_State *L, const char *buff, size_t sz, const char *name)
{
    int api = 0;
    return luaL_loadbufferx_worker(api, L, buff, sz, name, NULL);
}

static int luaL_loadbufferx_intercept(lua_State *L, const char *buff, size_t sz, const char *name, const char* mode)
{
    int api = 0;
    return luaL_loadbufferx_worker(api, L, buff, sz, name, mode);
}

static lua_State* lua_open_500_intercept()
{
    int api = 0;
    return lua_open_500_worker(api);
}

static int luaL_loadfile_intercept(lua_State *L, const char *fileName)
{
    int api = 0;
    return luaL_loadfilex_worker(api, L, fileName, NULL);
}

static int luaL_loadfilex_intercept(lua_State *L, const char *fileName, const char* mode)
{
    int api = 0;
    return luaL_loadfilex_worker(api, L, fileName, mode);
}

bool LoadLuaFunctions(const std::unordered_map<std::string, DWORD64>& symbols, HANDLE hProcess)
{

    #define GET_FUNCTION_OPTIONAL(function)                                                                                     \
        {                                                                                                                       \
            std::unordered_map<std::string, DWORD64>::const_iterator iterator = symbols.find(#function);                          \
            if (iterator != symbols.end())                                                                                      \
            {                                                                                                                   \
                luaInterface.function##_dll.SetFunction(reinterpret_cast<void*>(iterator->second));                     \
            }                                                                                                                   \
        }

    #define GET_FUNCTION(function)                                                                                              \
        GET_FUNCTION_OPTIONAL(function)                                                                                         \
        if (!luaInterface.function##_dll)                                                                          \
        {                                                                                                                       \
            if (report)                                                                                                         \
            {                                                                                                                   \
                DebugBackend::Get().Message("Warning 1004: Couldn't hook Lua function '" #function "'", MessageType_Warning);   \
            }                                                                                                                   \
            return false;                                                                                                       \
        }

    #define HOOK_FUNCTION(function)                                                                                             \
        if (luaInterface.function##_dll)                                                                          \
        {                                                                                                                       \
            void* original = luaInterface.function##_dll.cdeclptr;                                                                 \
            luaInterface.function##_dll.SetFunction(HookFunction(original, function##_intercept, api));        \
        }

  #define HOOK_FUNCTION_DETOURS(function)                                                                                             \
        if (luaInterface.function##_dll)                                                                          \
        {                                                                                                                       \
            void* original = luaInterface.function##_dll_cdecl;                                                                 \
            luaInterface.function##_dll.SetFunction(HookFunction_Detours(original, function##_intercept, api));        \
        }

    LuaInterface luaInterface = { 0 };
    luaInterface.finishedLoading = false;
    luaInterface.stdcall         = false;

    int api = (int)g_interfaces.size();

    bool report = false;

    // Check if the lua_tag function exists. This function is only in Lua 4.0 and not in Lua 5.0.
    // This helps us differentiate between those two versions.

    luaInterface.registryIndex  = 0;
    luaInterface.globalsIndex   = 0;
    luaInterface.hookTailCall = -1;
    luaInterface.hookTailRet = -1;

    if (symbols.find("lua_tag") != symbols.end())
    {
        luaInterface.version = 401;
    }
    else
    {
        if (symbols.find("lua_open") != symbols.end())
        {
            luaInterface.version        = 500;
            luaInterface.registryIndex  = -10000;
            luaInterface.globalsIndex   = -10001;
        }
        else if (symbols.find("lua_callk") != symbols.end())
        {
            luaInterface.version        = 520;
            // LUA_REGISTRYINDEX == LUAI_FIRSTPSEUDOIDX with LUAI_FIRSTPSEUDOIDX == (-LUAI_MAXSTACK - 1000) with LUAI_MAXSTACK == 15000 (for 32 bits build...)
            luaInterface.registryIndex  = -1001000;
            // starting with Lua 5.2, there is no longer a LUA_GLOBALSINDEX pseudo-index. Instead the global table is stored in the registry at LUA_RIDX_GLOBALS
            luaInterface.globalsIndex   = 2;
            luaInterface.hookTailCall = LUA_HOOKTAILCALL; // Lua5.2 has LUA_HOOKTAILCALL, but no LUA_HOOKTAILRET
        }
        else if ( symbols.find("lua_gettop") != symbols.end()) // should be ok for any version
        {
            luaInterface.version        = 510;
            luaInterface.registryIndex  = -10000;
            luaInterface.globalsIndex   = -10002;
            luaInterface.hookTailRet = LUA_HOOKTAILRET; // // Lua5.1 has LUA_HOOKTAILRET, but no LUA_HOOKTAILCALL
        }
        else // if we get here, this means the module isn't related to Lua at all
        {
            return false;
        }
    }
    
    // Only present in Lua 4.0 and Lua 5.0 (not 5.1)
    GET_FUNCTION_OPTIONAL(lua_open);
    if (!luaInterface.lua_open_dll.GetFunction())
    {
        GET_FUNCTION(lua_newstate);
    }

    // Start reporting errors about functions we couldn't hook.
    report = true;
    
    GET_FUNCTION(lua_newthread);
    GET_FUNCTION(lua_close);
    GET_FUNCTION(lua_error);
    GET_FUNCTION_OPTIONAL(lua_absindex); // Only present in Lua 5.2+
    GET_FUNCTION(lua_sethook);
    GET_FUNCTION(lua_getinfo);
    GET_FUNCTION(lua_remove);
    GET_FUNCTION(lua_settable);
    GET_FUNCTION(lua_gettable);
    GET_FUNCTION(lua_rawget);
    GET_FUNCTION(lua_rawgeti);
    GET_FUNCTION(lua_rawset);
    GET_FUNCTION(lua_pushstring);
    GET_FUNCTION(lua_pushlstring);
    GET_FUNCTION(lua_type);
    GET_FUNCTION(lua_typename);
    GET_FUNCTION(lua_settop);
    GET_FUNCTION(lua_gettop);
    GET_FUNCTION(lua_getlocal);
    GET_FUNCTION(lua_setlocal);
    GET_FUNCTION(lua_getstack);
    GET_FUNCTION(lua_insert);
    GET_FUNCTION(lua_pushnil);
    GET_FUNCTION(lua_pushvalue);
    GET_FUNCTION(lua_pushcclosure);
    GET_FUNCTION(lua_pushnumber);
    GET_FUNCTION(lua_pushlightuserdata);
    GET_FUNCTION(lua_checkstack);
    GET_FUNCTION(lua_gethookmask);

    // Only present in Lua 5.2+
    GET_FUNCTION_OPTIONAL(lua_getglobal);
    GET_FUNCTION_OPTIONAL(lua_setglobal);

    // Only present in Lua 5.1 (*number funtions used in Lua 4.0)
    GET_FUNCTION_OPTIONAL(lua_pushinteger);
    GET_FUNCTION_OPTIONAL(lua_tointeger);
    GET_FUNCTION_OPTIONAL(lua_tointegerx);

    // Only present in Lua 4.0 and 5.0 (exists as a macro in Lua 5.1)
    GET_FUNCTION_OPTIONAL(lua_tostring);

    if (!luaInterface.lua_tostring_dll)
    {
        GET_FUNCTION(lua_tolstring);
    }

    GET_FUNCTION_OPTIONAL(lua_tonumberx);

    if (!luaInterface.lua_tonumberx_dll)
    {
        // If the Lua 5.2 tonumber isn't present, require the previous version.
        GET_FUNCTION(lua_tonumber);
    }

    GET_FUNCTION(lua_toboolean);
    GET_FUNCTION(lua_tocfunction);
    GET_FUNCTION(lua_touserdata);
    
    // Exists as a macro in Lua 5.2
    GET_FUNCTION_OPTIONAL(lua_callk);
    if (!luaInterface.lua_callk_dll)
    {
        GET_FUNCTION(lua_call);
    }
    
    // Exists as a macro in Lua 5.2
    GET_FUNCTION_OPTIONAL(lua_pcallk);
    if (!luaInterface.lua_pcallk_dll)
    {
        GET_FUNCTION(lua_pcall);
    }
    
    // Only present in Lua 4.0 and 5.0 (exists as a macro in Lua 5.1)
    GET_FUNCTION_OPTIONAL(lua_newtable);
    if (!luaInterface.lua_newtable_dll)
    {
        GET_FUNCTION(lua_createtable);
    }

    GET_FUNCTION(lua_load);
    GET_FUNCTION(lua_next);
    GET_FUNCTION(lua_rawequal);
    GET_FUNCTION(lua_getmetatable);
    GET_FUNCTION(lua_setmetatable);
    GET_FUNCTION_OPTIONAL(luaL_ref);
    GET_FUNCTION_OPTIONAL(luaL_unref);
    GET_FUNCTION(luaL_newmetatable);
    GET_FUNCTION(lua_getupvalue);
    GET_FUNCTION(lua_setupvalue);

    // Only available in Lua 5.0/5.1
    GET_FUNCTION_OPTIONAL(lua_getfenv);
    GET_FUNCTION_OPTIONAL(lua_setfenv);

    if (luaInterface.version >= 510)
    {
        GET_FUNCTION(lua_pushthread);
    }
    else
    {
        // This function doesn't exist in Lua 5.0, so make it optional.
        GET_FUNCTION_OPTIONAL(lua_pushthread);
    }

    GET_FUNCTION(lua_newuserdata);

    // This function isn't strictly necessary. We only hook it
    // in case the base function was inlined.
    GET_FUNCTION_OPTIONAL(luaL_newstate);
    GET_FUNCTION_OPTIONAL(luaL_loadbuffer);
    GET_FUNCTION_OPTIONAL(luaL_loadfile);
    GET_FUNCTION_OPTIONAL(luaL_loadfilex);

    // These functions only exists in LuaPlus.
    GET_FUNCTION_OPTIONAL(lua_towstring);
    GET_FUNCTION_OPTIONAL(lua_iswstring);

    // Hook the functions we need to intercept calls to.

    if (luaInterface.version == 500)
    {
        luaInterface.lua_open_500_dll.SetFunction(luaInterface.lua_load_dll);
        luaInterface.lua_load_dll.SetFunction(NULL);
    }

    if (luaInterface.version == 510)
    {
        luaInterface.lua_load_510_dll.SetFunction(luaInterface.lua_load_dll);
        luaInterface.lua_load_dll.SetFunction(NULL);
    }

    HOOK_FUNCTION(lua_open);
    HOOK_FUNCTION(lua_open_500);
    HOOK_FUNCTION(lua_newstate);
    HOOK_FUNCTION(lua_close);
    HOOK_FUNCTION(lua_newthread);
    HOOK_FUNCTION(lua_pcall);
    HOOK_FUNCTION(lua_pcallk);
    HOOK_FUNCTION(lua_call);
    HOOK_FUNCTION(lua_callk);
    HOOK_FUNCTION(lua_load);
    HOOK_FUNCTION(lua_load_510);
    HOOK_FUNCTION(luaL_newmetatable);
    HOOK_FUNCTION(lua_sethook);

    HOOK_FUNCTION(luaL_loadbuffer);
    HOOK_FUNCTION(luaL_loadfile);
    HOOK_FUNCTION(luaL_loadfilex);
    HOOK_FUNCTION(luaL_newstate);

#ifdef VERBOSE
    DebugBackend::Get().Message("Found all necessary Lua functions");
#endif
    
    // Setup our API.

    luaInterface.DecodaOutput  = (lua_CFunction)InstanceFunction(DecodaOutput, api);
    luaInterface.CPCallHandler = (lua_CFunction)InstanceFunction(CPCallHandler, api);
    luaInterface.HookHandler   = (lua_Hook)InstanceFunction(HookHandler, api);

    g_interfaces.push_back( luaInterface );

    if (!g_loadedLuaFunctions)
    {
        DebugBackend::Get().Message("Debugger attached to process");
        g_loadedLuaFunctions = true;
    }

    return true;

}

static PIMAGE_NT_HEADERS PEHeaderFromHModule(HMODULE hModule)
{
    PIMAGE_NT_HEADERS pNTHeader = 0;
    
    __try
    {
        if ( PIMAGE_DOS_HEADER(hModule)->e_magic != IMAGE_DOS_SIGNATURE )
            __leave;

        pNTHeader = PIMAGE_NT_HEADERS(PBYTE(hModule)
                    + PIMAGE_DOS_HEADER(hModule)->e_lfanew);
        
        if ( pNTHeader->Signature != IMAGE_NT_SIGNATURE )
            pNTHeader = 0;
    }
    __except( EXCEPTION_EXECUTE_HANDLER )
    {       
    }

    return pNTHeader;
}

/**
 * Gets a list of the files that are imported by a module.
 */
bool GetModuleImports(HANDLE hProcess, HMODULE hModule, std::vector<std::string>& imports)
{

    PIMAGE_NT_HEADERS pExeNTHdr = PEHeaderFromHModule( hModule );
    
    if ( !pExeNTHdr )
    {
        return false;
    }

    DWORD importRVA = pExeNTHdr->OptionalHeader.DataDirectory
                        [IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress;
    if ( !importRVA )
    {
        return false;
    }

    // Convert imports RVA to a usable pointer
    PIMAGE_IMPORT_DESCRIPTOR pImportDesc = MAKE_PTR( PIMAGE_IMPORT_DESCRIPTOR,
                                                    hModule, importRVA );

    // Iterate through each import descriptor, and redirect if appropriate
    while ( pImportDesc->FirstThunk )
    {
        PSTR pszImportModuleName = MAKE_PTR( PSTR, hModule, pImportDesc->Name);
        imports.push_back(pszImportModuleName);
        pImportDesc++;  // Advance to next import descriptor
    }

    return true;
}

bool GetFileExists(const char* fileName)
{
    return GetFileAttributes(fileName) != INVALID_FILE_ATTRIBUTES;
}

void ReplaceExtension(char fileName[_MAX_PATH], const char* extension)
{

    char* start = strrchr(fileName, '.');

    if (start == NULL)
    {
        strcat(fileName, extension);
    }
    else
    {
        strcpy(start + 1, extension);
    }

}

void GetFileTitle(const char* fileName, char fileTitle[_MAX_PATH])
{

    const char* slash1 = strrchr(fileName, '\\');
    const char* slash2 = strrchr(fileName, '/');

    const char* pathEnd = max(slash1, slash2);

    if (pathEnd == NULL)
    {
        // There's no path so the whole thing is the file title.
        strcpy(fileTitle, fileName);
    }
    else
    {
        strcpy(fileTitle, pathEnd + 1);
    }

}

void GetFilePath(const char* fileName, char path[_MAX_PATH])
{

    const char* slash1 = strrchr(fileName, '\\');
    const char* slash2 = strrchr(fileName, '/');

    const char* pathEnd = max(slash1, slash2);

    if (pathEnd == NULL)
    {
        // There's no path on the file name.
        path[0] = 0;
    }
    else
    {
        size_t length = pathEnd - fileName + 1;
        memcpy(path, fileName, length);
        path[length] = 0;
    }

}

bool LocateSymbolFile(const IMAGEHLP_MODULE64& moduleInfo, char fileName[_MAX_PATH])
{

    // The search order for symbol files is described here:
    // http://msdn2.microsoft.com/en-us/library/ms680689.aspx

    // This function doesn't currently support the full spec.

    const char* imageFileName = moduleInfo.LoadedImageName;
    
    // First check the absolute path specified in the CodeView data.
    if (GetFileExists(moduleInfo.CVData))
    {
        strncpy(fileName, moduleInfo.CVData, _MAX_PATH);
        return true;
    }

    char symbolTitle[_MAX_PATH];
    GetFileTitle(moduleInfo.CVData, symbolTitle);

    // Now check in the same directory as the image.
    
    char imagePath[_MAX_PATH];
    GetFilePath(imageFileName, imagePath);

    strcat(imagePath, symbolTitle);
    
    if (GetFileExists(imagePath))
    {
        strncpy(fileName, imagePath, _MAX_PATH);
        return true;
    }

    return false;

}

BOOL CALLBACK GatherSymbolsCallback(PSYMBOL_INFO pSymInfo, ULONG SymbolSize, PVOID UserContext)
{
    
    std::unordered_map<std::string, DWORD64>* symbols = reinterpret_cast<std::unordered_map<std::string, DWORD64>*>(UserContext);

    if (pSymInfo != NULL && pSymInfo->Name != NULL)
    {
        symbols->insert(std::make_pair(pSymInfo->Name, pSymInfo->Address));
    }
    
    return TRUE;

}

BOOL CALLBACK FindSymbolsCallback(PSYMBOL_INFO pSymInfo, ULONG SymbolSize, PVOID UserContext)
{
    bool* found = reinterpret_cast<bool*>(UserContext);
    *found = true;
    return FALSE;
}

bool ScanForSignature(DWORD64 start, DWORD64 length, const char* signature)
{

    size_t signatureLength = strlen(signature);

    for (DWORD64 i = start; i < start + length - signatureLength; ++i)
    {

        void* p = reinterpret_cast<void*>(i);

        // Check that we have read access to the data. For some reason under Windows
        // Vista part of the DLL is not accessible (possibly some sort of new delay
        // loading mechanism for DLLs?)
        if (IsBadReadPtr(reinterpret_cast<LPCSTR>(p), signatureLength))
        {
            break;
        }

        if (memcmp(p, signature, signatureLength) == 0)
        {
            return true;
        }

    }

    return false;

}

void LoadSymbolsRecursively(std::set<std::string>& loadedModules, std::unordered_map<std::string, DWORD64>& symbols, HANDLE hProcess, HMODULE hModule)
{

    assert(hModule != NULL);

    char moduleName[_MAX_PATH];
    GetModuleBaseName(hProcess, hModule, moduleName, _MAX_PATH);

    if (loadedModules.find(moduleName) == loadedModules.end())
    {

        // Record that we've loaded this module so that we don't
        // try to load it again.
        loadedModules.insert(moduleName);

        MODULEINFO moduleInfo = { 0 };
        GetModuleInformation(hProcess, hModule, &moduleInfo, sizeof(moduleInfo));

        char moduleFileName[_MAX_PATH];
        GetModuleFileNameEx(hProcess, hModule, moduleFileName, _MAX_PATH);
        
        DWORD64 base = SymLoadModule64_dll(hProcess, NULL, moduleFileName, moduleName, (DWORD64)moduleInfo.lpBaseOfDll, moduleInfo.SizeOfImage);

        #ifdef VERBOSE
            char message[1024];
            _snprintf(message, 1024, "Examining '%s' %s\n", moduleName, base ? "(symbols loaded)" : "");
            DebugBackend::Get().Log(message);
        #endif

        // Check to see if there was a symbol file we failed to load (usually
        // becase it didn't match the version of the module).
            
        IMAGEHLP_MODULE64 module;
        memset(&module, 0, sizeof(module));
        module.SizeOfStruct = sizeof(module);

        BOOL result = SymGetModuleInfo64_dll(hProcess, base, &module);

        if (result && module.SymType == SymNone)
        {

            // No symbols were found. Check to see if the module file name + ".pdb"
            // exists, since the symbol file and/or module names may have been renamed.
            
            char pdbFileName[_MAX_PATH];
            strcpy(pdbFileName, moduleFileName);
            ReplaceExtension(pdbFileName, "pdb");
            
            if (GetFileExists(pdbFileName))
            {
                
                SymUnloadModule64_dll(hProcess, base);
                base = SymLoadModule64_dll(hProcess, NULL, pdbFileName, moduleName, (DWORD64)moduleInfo.lpBaseOfDll, moduleInfo.SizeOfImage);

                if (base != 0)
                {
                    result = SymGetModuleInfo64_dll(hProcess, base, &module);
                }
                else
                {
                    result = FALSE;
                }

            }

        }

        if (result)
        {

            // Check to see if we've already warned about this module.
            if (g_warnedAboutPdb.find(moduleFileName) == g_warnedAboutPdb.end())
            {
                if (strlen(module.CVData) > 0 && (module.SymType == SymExport || module.SymType == SymNone))
                {

                    char symbolFileName[_MAX_PATH];

                    if (LocateSymbolFile(module, symbolFileName))
                    {
                        char message[1024];
                        _snprintf(message, 1024, "Warning 1002: Symbol file '%s' located but it does not match module '%s'", symbolFileName, moduleFileName);
                        DebugBackend::Get().Message(message, MessageType_Warning);
                    }

                    // Remember that we've checked on this file, so no need to check again.
                    g_warnedAboutPdb.insert(moduleFileName);

                }
            }

        }
           
        if (base != 0)
        {
            // SymFromName is really slow, so we gather up our own list of the symbols that we
            // can index much faster.
            SymEnumSymbols_dll(hProcess, base, "lua*", GatherSymbolsCallback, reinterpret_cast<PVOID>(&symbols));
        }

        // Check to see if the module contains the Lua signature but we didn't find any Lua functions.

        if (g_warnedAboutLua.find(moduleFileName) == g_warnedAboutLua.end())
        {
            
            // Check to see if this module contains any Lua functions loaded from the symbols.

            bool foundLuaFunctions = false;

            if (base != 0)
            {
                SymEnumSymbols_dll(hProcess, base, "lua_*", FindSymbolsCallback, &foundLuaFunctions);
            }

            if (!foundLuaFunctions)
            {

                // Check to see if this module contains a string from the Lua source code. If it's there, it probably
                // means this module has Lua compiled into it.

                bool luaFile = ScanForSignature((DWORD64)hModule, moduleInfo.SizeOfImage, "$Lua:");

                if (luaFile)
                {
                    char message[1024];
                    _snprintf(message, 1024, "Warning 1001: '%s' appears to contain Lua functions however no Lua functions could located with the symbolic information", moduleFileName);
                    DebugBackend::Get().Message(message, MessageType_Warning);
                }

            }

            // Remember that we've checked on this file, so no need to check again.
            g_warnedAboutLua.insert(moduleFileName);

        }

        // Get the imports for the module. These are loaded before we're able to hook
        // LoadLibrary for the module.

        std::vector<std::string> imports;
        GetModuleImports(hProcess, hModule, imports);

        for (unsigned int i = 0; i < imports.size(); ++i)
        {

            HMODULE hImportModule = GetModuleHandle(imports[i].c_str());
            
            // Sometimes the import module comes back NULL, which means that for some reason
            // it wasn't loaded. Perhaps these are delay loaded and we'll catch them later?
            if (hImportModule != NULL)
            {
                LoadSymbolsRecursively(loadedModules, symbols, hProcess, hImportModule);
            }

        }

    }

}

BOOL CALLBACK SymbolCallbackFunction(HANDLE hProcess, ULONG code, ULONG64 data, ULONG64 UserContext)
{

    if (code == CBA_DEBUG_INFO)
    {
        DebugBackend::Get().Message(reinterpret_cast<char*>(data));
    }

    return TRUE;

}

void PostLoadLibrary(HMODULE hModule)
{

    extern HINSTANCE g_hInstance;

    if (hModule == g_hInstance)
    {
        // Don't investigate ourself.
        return;
    }

    HANDLE hProcess = GetCurrentProcess();

    char moduleName[_MAX_PATH];
    GetModuleBaseName(hProcess, hModule, moduleName, _MAX_PATH);

    CriticalSectionLock lock(g_loadedModulesCriticalSection);

    if (g_loadedModules.find(moduleName) == g_loadedModules.end())
    {

        // Record that we've loaded this module so that we don't
        // try to load it again.
        g_loadedModules.insert(moduleName);

        if (!g_initializedDebugHelp)
        {
            if (!SymInitialize_dll(hProcess, g_symbolsDirectory.c_str(), FALSE))
            {
                return;
            }
            g_initializedDebugHelp = true;
        }

        //SymSetOptions(SYMOPT_DEBUG);

        std::set<std::string> loadedModules;
        std::unordered_map<std::string, DWORD64> symbols;

        LoadSymbolsRecursively(loadedModules, symbols, hProcess, hModule);

        LoadLuaFunctions(symbols, hProcess);

        //SymCleanup_dll(hProcess);
        //hProcess = NULL;
    
    }

}

void HookLoadLibrary()
{

    HMODULE hModuleKernel = GetModuleHandle("kernel32.dll");

    if (hModuleKernel != NULL)
    {
        // LoadLibraryExW is called by the other LoadLibrary functions, so we
        // only need to hook it.
        LoadLibraryExW_dll = (LoadLibraryExW_t) HookFunction( GetProcAddress(hModuleKernel, "LoadLibraryExW"), LoadLibraryExW_intercept);
    }

    // These NTDLL functions are undocumented and don't exist in Windows 2000.

    HMODULE hModuleNt = GetModuleHandle("ntdll.dll");

    if (hModuleNt != NULL)
    {
        LdrLockLoaderLock_dll   = (LdrLockLoaderLock_t)   GetProcAddress(hModuleNt, "LdrLockLoaderLock");
        LdrUnlockLoaderLock_dll = (LdrUnlockLoaderLock_t) GetProcAddress(hModuleNt, "LdrUnlockLoaderLock");
    }

}

bool InstallLuaHooker(HINSTANCE hInstance, const char* symbolsDirectory)
{

    // Load the dbghelp functions. We have to do this dynamically since the
    // older version of dbghelp that ships with Windows doesn't successfully
    // load the symbols from PDBs. We can't simply include our new DLL since
    // it needs to be in the directory for the application we're *debugging*
    // since this DLL is injected.
    if (!LoadDebugHelp(hInstance))
    {
        return false;
    }

    g_symbolsDirectory = symbolsDirectory;

    // Add the "standard" stuff to the symbols directory search path.
    g_symbolsDirectory += ";" + GetApplicationDirectory();
    g_symbolsDirectory += ";" + GetEnvironmentVariable("_NT_SYMBOL_PATH");
    g_symbolsDirectory += ";" + GetEnvironmentVariable("_NT_ALTERNATE_SYMBOL_PATH");

    // Hook LoadLibrary* functions so that we can intercept those calls and search
    // for Lua functions.
    HookLoadLibrary();

    // Avoid deadlock if a new DLL is loaded during this function.
    ULONG cookie;

    if (LdrLockLoaderLock_dll   != NULL &&
        LdrUnlockLoaderLock_dll != NULL)
    {
        LdrLockLoaderLock_dll(0, 0, &cookie);
    }

    // Process all of the loaded modules.

    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, 0);

    if (hSnapshot == NULL)
    {

        // If for some reason we couldn't take a snapshot, just load the
        // main module. This shouldn't ever happen, but we do it just in
        // case.
        HMODULE hModule = GetModuleHandle(NULL);
        PostLoadLibrary(hModule);

        if (LdrLockLoaderLock_dll   != NULL &&
            LdrUnlockLoaderLock_dll != NULL)
        {
            LdrUnlockLoaderLock_dll(0, cookie);
        }

        return true;

    }

    MODULEENTRY32 module;
    module.dwSize = sizeof(MODULEENTRY32);

    BOOL moreModules = Module32First(hSnapshot, &module);

    while (moreModules)
    {
        PostLoadLibrary(module.hModule);
        moreModules = Module32Next(hSnapshot, &module);
    }

    CloseHandle(hSnapshot);
    hSnapshot = NULL;

    if (LdrLockLoaderLock_dll   != NULL &&
        LdrUnlockLoaderLock_dll != NULL)
    {
        LdrUnlockLoaderLock_dll(0, cookie);
    }

    return true;

}

bool GetIsLuaLoaded()
{
    return g_loadedLuaFunctions;
}

struct CFunctionArgs
{
    unsigned long       api;
    lua_CFunction_dll   function;
};

#pragma auto_inline(off)
int CFunctionHandlerWorker(CFunctionArgs* args, lua_State* L)
{
    return args->function(args->api, L);
}
#pragma auto_inline()

NAKED int CFunctionHandler(CFunctionArgs* args, lua_State* L)
{

    int result;
    bool stdcall;
        
    INTERCEPT_PROLOG()
    
    stdcall = false;
    result = CFunctionHandlerWorker(args, L);

    INTERCEPT_EPILOG(4)

}

lua_CFunction CreateCFunction(int api, lua_CFunction_dll function)
{
    
    // This is never deallocated, but it doesn't really matter since we never
    // destroy these functions.
    CFunctionArgs* args = new CFunctionArgs;
    args->api       = api;
    args->function  = function;

    return (lua_CFunction)InstanceFunction(CFunctionHandler, reinterpret_cast<unsigned long>(args));
    
}
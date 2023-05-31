// Copyright (c) 2023 Christopher Antos
// License: http://opensource.org/licenses/MIT

#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commctrl.h>
#include "dpi.h"
#include <string>
#include <assert.h>

LONG ReadRegLong(const WCHAR* name, LONG default_value);
void WriteRegLong(const WCHAR* name, LONG value);

extern bool g_use_compressed_size;
extern bool g_show_free_space;
extern bool g_show_names;
extern bool g_show_comparison_bar;
extern long g_color_mode;
enum ColorMode { CM_PLAIN, CM_RAINBOW, CM_HEATMAP };
#ifdef DEBUG
extern long g_fake_data;
enum FakeDataMode { FDM_REAL, FDM_SIMULATED, FDM_COLORWHEEL, FDM_EMPTYDRIVE, FDM_ONLYDIRS };
#endif

void MakeMenuPretty(HMENU hmenu);

enum class UnitScale { Auto, KB, MB, GB };
UnitScale AutoUnitScale(ULONGLONG size);
void FormatSize(ULONGLONG size, std::wstring& text, std::wstring& units, UnitScale scale=UnitScale::Auto, int places=-1);
void FormatCount(ULONGLONG count, std::wstring& text);

//----------------------------------------------------------------------------
// Smart pointer for AddRef/Release refcounting.

template<class T> struct RemoveConst { typedef T type; };
template<class T> struct RemoveConst<T const> { typedef T type; };

template<class IFace>
class PrivateRelease : public IFace
{
private:
    STDMETHODIMP_(ULONG) Release(); // Force Release to be private to prevent "spfoo->Release()".
    ~PrivateRelease(); // Avoid compiler warning/error when destructor is private.
};

// Using a macro for this allows you to give your editor's tagging engine a
// preprocessor hint to help it not get confused.
#define SPI_TAGGINGTRICK(IFace) PrivateRelease<IFace>

template<class IFace, class ICast = IFace>
class SPI
{
    typedef typename RemoveConst<ICast>::type ICastRemoveConst;

public:
    SPI()                           { m_p = 0; }
#if 0
    // Disabled because of ambiguity between "SPI<Foo> spFoo = new Foo"
    // and "SPI<Foo> spFoo( spPtrToCopy )".  One should not AddRef, but
    // the other sometimes should and sometimes should not.  But both end
    // up using the constructor.  So for now we can't allow either form.
    SPI(IFace *p)                   { m_p = p; if (m_p) m_p->AddRef(); }
#endif
    ~SPI()                          { if (m_p) RemoveConst(m_p)->Release(); }
    SPI(SPI<IFace,ICast>&& other)   { m_p = other.m_p; other.m_p = 0; }
    operator IFace*() const         { return m_p; }
    SPI_TAGGINGTRICK(IFace)* Pointer() const { return static_cast<PrivateRelease<IFace>*>(m_p); }
    SPI_TAGGINGTRICK(IFace)* operator->() const { return static_cast<PrivateRelease<IFace>*>(RemoveConst(m_p)); }
    IFace** operator &()            { assert(!m_p); return &m_p; }
    IFace* operator=(IFace* p)      { assert(!m_p); return m_p = p; }
    IFace* operator=(SPI<IFace,ICast>&& other) { assert(!m_p); m_p = other.m_p; other.m_p = 0; return m_p; }
    IFace* Transfer()               { return Detach(); }
    IFace* Copy() const             { if (m_p) RemoveConst(m_p)->AddRef(); return m_p; }
    void Release()                  { Attach(0); }
    void Set(IFace* p)              { if (p && m_p != p) RemoveConst(p)->AddRef(); Attach(p); }
    void Attach(IFace* p)           { ICast* pRelease = static_cast<ICast*>(m_p); m_p = p; if (pRelease && pRelease != p) RemoveConst(pRelease)->Release(); }
    IFace* Detach()                 { IFace* p = m_p; m_p = 0; return p; }
    bool operator!()                { return !m_p; }

    IFace** UnsafeAddress()         { return &m_p; }

protected:
    static ICastRemoveConst*        RemoveConst(ICast* p) { return const_cast<ICastRemoveConst*>(static_cast<ICast*>(p)); }

protected:
    IFace* m_p;

private:
    SPI<IFace,ICast>& operator=(SPI<IFace,ICast> const& sp) = delete;
    SPI(SPI<IFace,ICast> const&) = delete;
};

struct IUnknown;

template<class IFace, IID const& iid=__uuidof(IFace)>
class SPQI : public SPI<IFace>
{
public:
    SPQI() : SPI<IFace>()           {}
#if 0
    // See comment in SPI...
    SPQI(IFace* p) : SPI<IFace>(p)  {}
#endif
    SPQI(SPQI<IFace,iid>&& other)   { SPI<IFace>::operator=(std::move(other)); }

    bool FQuery(IUnknown* punk)     { return SUCCEEDED(HrQuery(punk)); }
    HRESULT HrQuery(IUnknown* punk) { assert(!m_p); return punk->QueryInterface(iid, (void**)&m_p); }

    IFace* operator=(IFace* p)      { return SPI<IFace>::operator=(p); }
    IFace* operator=(SPQI<IFace,iid>&& other) { return SPI<IFace>::operator=(std::move(other)); }

private:
    SPQI<IFace, iid>& operator=(SPQI<IFace, iid> const& sp) = delete;
    SPQI(SPQI<IFace, iid> const&) = delete;
};

//----------------------------------------------------------------------------
// Smart handle.

// NOTE: a compiler bug forces us to use DWORD_PTR instead of Type.
template<class Type, DWORD_PTR EmptyValue, class Subclass>
class SH : public Subclass
{
public:
    SH(Type h = Type(EmptyValue))   { m_h = h; }
    ~SH()                           { if (Type(EmptyValue) != m_h) Subclass::Free(m_h); }
    SH(SH<Type,EmptyValue,Subclass>&& other) { m_h = other.m_h; other.m_h = Type(EmptyValue); }
    operator Type() const           { return m_h; }
    Type Handle() const             { return m_h; }
    Type* operator&()               { assert(Type(EmptyValue) == m_h); return &m_h; }
    Type operator=(Type h)          { assert(Type(EmptyValue) == m_h); return m_h = h; }
    Type operator=(SH<Type,EmptyValue,Subclass>&& other) { assert(Type(EmptyValue) == m_h); m_h = other.m_h; other.m_h = 0; return m_h; }
    void Set(Type h)                { if (Type(EmptyValue) != m_h) Subclass::Free(m_h); m_h = h; }
    Type Transfer()                 { return Detach(); }
    void Free()                     { if (Type(EmptyValue) != m_h) Subclass::Free(m_h); m_h = Type(EmptyValue); }
    void Close()                    { Free(); }
    void Attach(Type h)             { if (m_h != h) Free(); m_h = h; }
    Type Detach()                   { Type h = m_h; m_h = Type(EmptyValue); return h; }
    bool operator!() const          { static_assert(EmptyValue == 0, "operator! requires empty value == 0"); return !m_h; }
    bool IsEmpty() const            { return EmptyValue == reinterpret_cast<DWORD_PTR>(m_h); }

    Type* UnsafeAddress()           { return &m_h; }

protected:
    Type m_h;

private:
    SH<Type, EmptyValue, Subclass> &operator=(SH<Type, EmptyValue, Subclass> const& sh) = delete;
    SH(SH<Type, EmptyValue, Subclass> const&) = delete;
};

class SH_CloseHandle { protected: void Free(HANDLE h) { CloseHandle(h); } };
class SH_FindClose { protected: void Free(HANDLE h) { FindClose(h); } };
class SH_RegCloseKey { protected: void Free(HKEY hkey) { RegCloseKey(hkey); } };
class SH_DeleteObject { protected: void Free(HGDIOBJ hobj) { DeleteObject(hobj); } };
class SH_DestroyCursor { protected: void Free(HCURSOR hcur) { DestroyCursor(hcur); } };
class SH_DestroyIcon { protected: void Free(HICON hicon) { DestroyIcon(hicon); } };

typedef SH<HANDLE, NULL, SH_CloseHandle> SHandle;
typedef SH<HANDLE, DWORD_PTR(INVALID_HANDLE_VALUE), SH_CloseHandle> SFileHandle;
typedef SH<HANDLE, DWORD_PTR(INVALID_HANDLE_VALUE), SH_FindClose> SFindHandle;
typedef SH<HKEY, NULL, SH_RegCloseKey> SHKEY;
typedef SH<HPEN, NULL, SH_DeleteObject> SHPEN;
typedef SH<HBRUSH, NULL, SH_DeleteObject> SHBRUSH;
typedef SH<HBITMAP, NULL, SH_DeleteObject> SHBITMAP;
typedef SH<HFONT, NULL, SH_DeleteObject> SHFONT;
typedef SH<HCURSOR, NULL, SH_DestroyCursor> SHCURSOR;
typedef SH<HICON, NULL, SH_DestroyIcon> SHICON;


#ifndef BASE_COM_PTR_H_
#define BASE_COM_PTR_H_

template <class T>
class _NoAddRefReleaseOnCComPtr : public T {
private:
  STDMETHOD_(ULONG, AddRef)()=0;
  STDMETHOD_(ULONG, Release)()=0;
};


template <class T>
class CComPtr {
public:
  CComPtr() : _p(NULL) {}
  CComPtr(T *x) : _p(x) { if (x) x->AddRef(); }
  ~CComPtr() {
    if (_p)
      _p->Release();
  }
  operator T*() const { return _p; }
  T *get() const { return _p; }
  T& operator*() const { return *_p; }
  T** operator&() { 
    //DCHECK(_p == NULL);
    return &_p; }
  bool operator!() const throw() { return _p == NULL; }
  _NoAddRefReleaseOnCComPtr<T>* operator->() const throw() {
    //DCHECK(_p != NULL);
    return (_NoAddRefReleaseOnCComPtr<T>*)_p;
  }
  HRESULT CoCreateInstance(__in REFCLSID rclsid, __in_opt LPUNKNOWN pUnkOuter = NULL, __in DWORD dwClsContext = CLSCTX_ALL) throw() {
    //DCHECK(_p == NULL);
    return ::CoCreateInstance(rclsid, pUnkOuter, dwClsContext, __uuidof(T), (void**)&_p);
  }

  // Release the interface and set to NULL
  void Release() throw() {
    T* pTemp = _p;
    if (pTemp) {
      _p = NULL;
      pTemp->Release();
    }
  }

  // Attach to an existing interface (does not AddRef)
  void Attach(T* p2) throw() {
    if (_p)
      _p->Release();
    _p = p2;
  }

  // Detach the interface (does not Release)
  T* Detach() throw() {
    T* pt = _p;
    _p = NULL;
    return pt;
  }


private:
  T *_p;
};
#endif  // BASE_COM_PTR_H_

#include <winapifamily.h>
#include <unknwn.h>
#include <RTWorkQ.h>

#include <cstdio>

#define INVALID_WORK_QUEUE_ID 0xffffffff
DWORD g_WorkQueueId = INVALID_WORK_QUEUE_ID;
//#define MMCSS_AUDIO_CLASS    L"Audio"
//#define MMCSS_PROAUDIO_CLASS L"ProAudio"

class TestClass : public IRtwqAsyncCallback {
    STDMETHODIMP GetParameters(_Out_ DWORD* pdwFlags, _Out_ DWORD* pdwQueue);
    STDMETHODIMP Invoke(_In_ IRtwqAsyncResult* pAsyncResult);
    virtual HRESULT STDMETHODCALLTYPE QueryInterface(
        /* [in] */ REFIID riid,
        /* [iid_is][out] */ _COM_Outptr_ void __RPC_FAR* __RPC_FAR* ppvObject){
        if (riid == __uuidof(IRtwqAsyncCallback) || riid == __uuidof(IUnknown)) {
            *ppvObject = this;
            return S_OK;
        }
        else {
            *ppvObject = nullptr;
            return E_UNEXPECTED;
        }
    }

    virtual ULONG STDMETHODCALLTYPE AddRef(){
        refs++;
        return refs;
    };

    virtual ULONG STDMETHODCALLTYPE Release(){
        refs--;
        return refs;
    }
    ULONG refs = 0;
};


STDMETHODIMP TestClass::GetParameters(DWORD* pdwFlags, DWORD* pdwQueue)
{
    HRESULT hr = S_OK;
    *pdwFlags = 0;
    *pdwQueue = g_WorkQueueId;
    return hr;
}

//-------------------------------------------------------
STDMETHODIMP TestClass::Invoke(IRtwqAsyncResult* pAsyncResult)
{
    HRESULT hr = S_OK;
    IUnknown* pState = NULL;
    WCHAR className[20];
    DWORD  bufferLength = 20;
    DWORD taskID = 0;
    LONG priority = 0;

    printf("Callback is invoked pAsyncResult(0x%0x)  Current process id :0x%0x Current thread id :0x%0x\n", (INT64)pAsyncResult, GetCurrentProcessId(), GetCurrentThreadId());

    hr = RtwqGetWorkQueueMMCSSClass(g_WorkQueueId, className, &bufferLength);
    if (FAILED(hr))
        exit(2);

    if (className[0]) {
        hr = RtwqGetWorkQueueMMCSSTaskId(g_WorkQueueId, &taskID);
        if (FAILED(hr))
            exit(3);

        hr = RtwqGetWorkQueueMMCSSPriority(g_WorkQueueId, &priority);
        if (FAILED(hr))
            exit(4);
        printf("MMCSS: [%ws] taskID (%d) priority(%d)\n", className, taskID, priority);
    }
    else {
        printf("non-MMCSS\n");
    }
    hr = pAsyncResult->GetState(&pState);
    if (FAILED(hr))
        exit(5);

Exit:
    return S_OK;
}
//-------------------------------------------------------

int main(int argc, char* argv[])
{
    HRESULT hr = S_OK;
    HANDLE signalEvent;
    LONG Priority = 1;
    IRtwqAsyncResult* pAsyncResult = NULL;
    RTWQWORKITEM_KEY workItemKey = NULL;;
    IRtwqAsyncCallback* callback = NULL;
    IUnknown* appObject = NULL;
    IUnknown* appState = NULL;
    DWORD taskId = 0;
    TestClass cbClass;
    NTSTATUS status;

    hr = RtwqStartup();
    if(FAILED(hr))
        exit(1);

    signalEvent = CreateEvent(NULL, true, FALSE, NULL);
    if (signalEvent == NULL) {
        hr = E_OUTOFMEMORY;
        exit(6);
    }

    g_WorkQueueId = RTWQ_MULTITHREADED_WORKQUEUE;

    hr = RtwqLockSharedWorkQueue(L"Audio", 0, &taskId, &g_WorkQueueId);
    if (FAILED(hr))
        exit(7);

    hr = RtwqCreateAsyncResult(NULL, reinterpret_cast<IRtwqAsyncCallback*>(&cbClass), NULL, &pAsyncResult);
    if (FAILED(hr))
        exit(8);

    hr = RtwqPutWaitingWorkItem(signalEvent, Priority, pAsyncResult, &workItemKey);
    if (FAILED(hr))
        exit(9);

    for (int i = 0; i < 5; i++) {
        SetEvent(signalEvent);
        Sleep(30);
        hr = RtwqPutWaitingWorkItem(signalEvent, Priority, pAsyncResult, &workItemKey);
        if (FAILED(hr))
            exit(10);
    }

Exit:
    if (pAsyncResult) {
        pAsyncResult->Release();
    }

    if (INVALID_WORK_QUEUE_ID != g_WorkQueueId) {
        hr = RtwqUnlockWorkQueue(g_WorkQueueId);
        if (FAILED(hr)) {
            printf("Failed with RtwqUnlockWorkQueue 0x%x\n", hr);
        }

        hr = RtwqShutdown();
        if (FAILED(hr)) {
            printf("Failed with RtwqShutdown 0x%x\n", hr);
        }
    }

    if (FAILED(hr)) {
        printf("Failed with error code 0x%x\n", hr);
    }
    return 0;
}
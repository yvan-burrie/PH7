/*
 * Copyright (C) 2011-2020 Symisc Systems. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Redistributions in any form must be accompanied by information on
 *    how to obtain complete source code for the PH7 engine and any
 *    accompanying software that uses the PH7 engine software.
 *    The source code must either be included in the distribution
 *    or be available for no more than the cost of distribution plus
 *    a nominal fee, and must be freely redistributable under reasonable
 *    conditions. For an executable file, complete source code means
 *    the source code for all modules it contains.It does not include
 *    source code for modules or files that typically accompany the major
 *    components of the operating system on which the executable file runs.
 *
 * THIS SOFTWARE IS PROVIDED BY SYMISC SYSTEMS ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, OR
 * NON-INFRINGEMENT, ARE DISCLAIMED.  IN NO EVENT SHALL SYMISC SYSTEMS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <ph7/ph7int.h>

#if defined(__WINNT__)
#include <Windows.h>
#else
#include <stdlib.h>
#endif

#if defined(PH7_ENABLE_THREADS)

/* SyRunTimeApi: sxmutex.c */
#if defined(__WINNT__)

struct SyMutex
{
    CRITICAL_SECTION sMutex;
    sxu32 nType; /* Mutex type,one of SXMUTEX_TYPE_* */
};

/* Preallocated static mutex */
static SyMutex aStaticMutexes[] = {
    {{0}, SXMUTEX_TYPE_STATIC_1},
    {{0}, SXMUTEX_TYPE_STATIC_2},
    {{0}, SXMUTEX_TYPE_STATIC_3},
    {{0}, SXMUTEX_TYPE_STATIC_4},
    {{0}, SXMUTEX_TYPE_STATIC_5},
    {{0}, SXMUTEX_TYPE_STATIC_6}
};
static BOOL winMutexInit = FALSE;
static LONG winMutexLock = 0;

static sxi32 WinMutexGlobaInit(void)
{
    LONG rc;
    rc = InterlockedCompareExchange(&winMutexLock, 1, 0);
    if (rc == 0)
    {
        sxu32 n;
        for (n = 0; n < SX_ARRAYSIZE(aStaticMutexes); ++n)
        {
            InitializeCriticalSection(&aStaticMutexes[n].sMutex);
        }
        winMutexInit = TRUE;
    }
    else
    {
        /* Someone else is doing this for us */
        while (winMutexInit == FALSE)
        {
            Sleep(1);
        }
    }
    return SXRET_OK;
}

static void WinMutexGlobalRelease(void)
{
    LONG rc;
    rc = InterlockedCompareExchange(&winMutexLock, 0, 1);
    if (rc == 1)
    {
        /* The first to decrement to zero does the actual global release */
        if (winMutexInit == TRUE)
        {
            sxu32 n;
            for (n = 0; n < SX_ARRAYSIZE(aStaticMutexes); ++n)
            {
                DeleteCriticalSection(&aStaticMutexes[n].sMutex);
            }
            winMutexInit = FALSE;
        }
    }
}

static SyMutex* WinMutexNew(int nType)
{
    SyMutex* pMutex = 0;
    if (nType == SXMUTEX_TYPE_FAST || nType == SXMUTEX_TYPE_RECURSIVE)
    {
        /* Allocate a new mutex */
        pMutex = (SyMutex*)HeapAlloc(GetProcessHeap(), 0, sizeof(SyMutex));
        if (pMutex == 0)
        {
            return 0;
        }
        InitializeCriticalSection(&pMutex->sMutex);
    }
    else
    {
        /* Use a pre-allocated static mutex */
        if (nType > SXMUTEX_TYPE_STATIC_6)
        {
            nType = SXMUTEX_TYPE_STATIC_6;
        }
        pMutex = &aStaticMutexes[nType - 3];
    }
    pMutex->nType = nType;
    return pMutex;
}

static void WinMutexRelease(SyMutex* pMutex)
{
    if (pMutex->nType == SXMUTEX_TYPE_FAST || pMutex->nType == SXMUTEX_TYPE_RECURSIVE)
    {
        DeleteCriticalSection(&pMutex->sMutex);
        HeapFree(GetProcessHeap(), 0, pMutex);
    }
}

static void WinMutexEnter(SyMutex* pMutex)
{
    EnterCriticalSection(&pMutex->sMutex);
}

static sxi32 WinMutexTryEnter(SyMutex* pMutex)
{
#ifdef _WIN32_WINNT
    BOOL rc;
    /* Only WindowsNT platforms */
    rc = TryEnterCriticalSection(&pMutex->sMutex);
    if (rc)
    {
        return SXRET_OK;
    }
    else
    {
        return SXERR_BUSY;
    }
#else
    return SXERR_NOTIMPLEMENTED;
#endif
}

static void WinMutexLeave(SyMutex* pMutex)
{
    LeaveCriticalSection(&pMutex->sMutex);
}

/* Export Windows mutex interfaces */
static const SyMutexMethods sWinMutexMethods = {
    WinMutexGlobaInit,  /* xGlobalInit() */
    WinMutexGlobalRelease, /* xGlobalRelease() */
    WinMutexNew,     /* xNew() */
    WinMutexRelease, /* xRelease() */
    WinMutexEnter,   /* xEnter() */
    WinMutexTryEnter, /* xTryEnter() */
    WinMutexLeave     /* xLeave() */
};

PH7_PRIVATE const SyMutexMethods* SyMutexExportMethods(void)
{
    return &sWinMutexMethods;
}

#elif defined(__UNIXES__)

#include <pthread.h>

struct SyMutex
{
    pthread_mutex_t sMutex;
    sxu32 nType;
};

static SyMutex* UnixMutexNew(int nType)
{
    static SyMutex aStaticMutexes[] = {
        {PTHREAD_MUTEX_INITIALIZER, SXMUTEX_TYPE_STATIC_1},
        {PTHREAD_MUTEX_INITIALIZER, SXMUTEX_TYPE_STATIC_2},
        {PTHREAD_MUTEX_INITIALIZER, SXMUTEX_TYPE_STATIC_3},
        {PTHREAD_MUTEX_INITIALIZER, SXMUTEX_TYPE_STATIC_4},
        {PTHREAD_MUTEX_INITIALIZER, SXMUTEX_TYPE_STATIC_5},
        {PTHREAD_MUTEX_INITIALIZER, SXMUTEX_TYPE_STATIC_6}
    };
    SyMutex* pMutex;

    if (nType == SXMUTEX_TYPE_FAST || nType == SXMUTEX_TYPE_RECURSIVE)
    {
        pthread_mutexattr_t sRecursiveAttr;
        /* Allocate a new mutex */
        pMutex = (SyMutex*)malloc(sizeof(SyMutex));
        if (pMutex == 0)
        {
            return 0;
        }
        if (nType == SXMUTEX_TYPE_RECURSIVE)
        {
            pthread_mutexattr_init(&sRecursiveAttr);
            pthread_mutexattr_settype(&sRecursiveAttr, PTHREAD_MUTEX_RECURSIVE);
        }
        pthread_mutex_init(&pMutex->sMutex, nType == SXMUTEX_TYPE_RECURSIVE ? &sRecursiveAttr : 0);
        if (nType == SXMUTEX_TYPE_RECURSIVE)
        {
            pthread_mutexattr_destroy(&sRecursiveAttr);
        }
    }
    else
    {
        /* Use a pre-allocated static mutex */
        if (nType > SXMUTEX_TYPE_STATIC_6)
        {
            nType = SXMUTEX_TYPE_STATIC_6;
        }
        pMutex = &aStaticMutexes[nType - 3];
    }
    pMutex->nType = nType;

    return pMutex;
}

static void UnixMutexRelease(SyMutex* pMutex)
{
    if (pMutex->nType == SXMUTEX_TYPE_FAST || pMutex->nType == SXMUTEX_TYPE_RECURSIVE)
    {
        pthread_mutex_destroy(&pMutex->sMutex);
        free(pMutex);
    }
}

static void UnixMutexEnter(SyMutex* pMutex)
{
    pthread_mutex_lock(&pMutex->sMutex);
}

static void UnixMutexLeave(SyMutex* pMutex)
{
    pthread_mutex_unlock(&pMutex->sMutex);
}

/* Export pthread mutex interfaces */
static const SyMutexMethods sPthreadMutexMethods = {
    0, /* xGlobalInit() */
    0, /* xGlobalRelease() */
    UnixMutexNew,      /* xNew() */
    UnixMutexRelease,  /* xRelease() */
    UnixMutexEnter,    /* xEnter() */
    0,                 /* xTryEnter() */
    UnixMutexLeave     /* xLeave() */
};

PH7_PRIVATE const SyMutexMethods* SyMutexExportMethods(void)
{
    return &sPthreadMutexMethods;
}

#else
/* Host application must register their own mutex subsystem if the target
 * platform is not an UNIX-like or windows systems.
 */
struct SyMutex
{
    sxu32 nType;
};

static SyMutex* DummyMutexNew(int nType)
{
    static SyMutex sMutex;
    SXUNUSED(nType);
    return &sMutex;
}

static void DummyMutexRelease(SyMutex* pMutex)
{
    SXUNUSED(pMutex);
}

static void DummyMutexEnter(SyMutex* pMutex)
{
    SXUNUSED(pMutex);
}

static void DummyMutexLeave(SyMutex* pMutex)
{
    SXUNUSED(pMutex);
}

// Export the dummy mutex interfaces.
static const SyMutexMethods sDummyMutexMethods = {
    0, /* xGlobalInit() */
    0, /* xGlobalRelease() */
    DummyMutexNew,      /* xNew() */
    DummyMutexRelease,  /* xRelease() */
    DummyMutexEnter,    /* xEnter() */
    0,                  /* xTryEnter() */
    DummyMutexLeave     /* xLeave() */
};

PH7_PRIVATE const SyMutexMethods* SyMutexExportMethods(void)
{
    return &sDummyMutexMethods;
}

#endif // __WINNT__
#endif // PH7_ENABLE_THREADS

static void* SyOSHeapAlloc(sxu32 nByte)
{
#if defined(__WINNT__)
    return HeapAlloc(GetProcessHeap(), 0, nByte);
#else
    return malloc((size_t)nByte);
#endif
}

static void* SyOSHeapRealloc(void* pOld, sxu32 nByte)
{
#if defined(__WINNT__)
    return HeapReAlloc(GetProcessHeap(), 0, pOld, nByte);
#else
    return realloc(pOld,(size_t)nByte);
#endif
}

static void SyOSHeapFree(void* pPtr)
{
#if defined(__WINNT__)
    HeapFree(GetProcessHeap(), 0, pPtr);
#else
    free(pPtr);
#endif
}

/* SyRunTimeApi:sxstr.c */
PH7_PRIVATE sxu32 SyStrlen(const char* zSrc)
{
#if defined(UNTRUST)
    if (zSrc == 0)
    {
        return 0;
    }
#endif

    register const char* zIn = zSrc;
    for (;;)
    {
        if (!zIn[0])
        {
            break;
        }
        zIn++;
        if (!zIn[0])
        {
            break;
        }
        zIn++;
        if (!zIn[0])
        {
            break;
        }
        zIn++;
        if (!zIn[0])
        {
            break;
        }
        zIn++;
    }

    return (sxu32)(zIn - zSrc);
}

PH7_PRIVATE sxi32 SyByteFind(const char* zStr, sxu32 nLen, sxi32 c, sxu32* pPos)
{
    const char* zIn = zStr;
    const char* zEnd = &zIn[nLen];
    for (;;)
    {
        if (zIn >= zEnd)
        {
            break;
        }
        if (zIn[0] == c)
        {
            if (pPos)
            {
                *pPos = (sxu32)(zIn - zStr);
            }

            return SXRET_OK;
        }
        zIn++;
        if (zIn >= zEnd)
        {
            break;
        }
        if (zIn[0] == c)
        {
            if (pPos)
            {
                *pPos = (sxu32)(zIn - zStr);
            }

            return SXRET_OK;
        }
        zIn++;
        if (zIn >= zEnd)
        {
            break;
        }
        if (zIn[0] == c)
        {
            if (pPos)
            {
                *pPos = (sxu32)(zIn - zStr);
            }

            return SXRET_OK;
        }
        zIn++;
        if (zIn >= zEnd)
        {
            break;
        }
        if (zIn[0] == c)
        {
            if (pPos)
            {
                *pPos = (sxu32)(zIn - zStr);
            }

            return SXRET_OK;
        }
        zIn++;
    }

    return SXERR_NOTFOUND;
}

#ifndef PH7_DISABLE_BUILTIN_FUNC

PH7_PRIVATE sxi32 SyByteFind2(const char* zStr, sxu32 nLen, sxi32 c, sxu32* pPos)
{
    const char* zIn = zStr;
    const char* zEnd;

    zEnd = &zIn[nLen - 1];
    for (;;)
    {
        if (zEnd < zIn)
        {
            break;
        }
        if (zEnd[0] == c)
        {
            if (pPos)
            {
                *pPos = (sxu32)(zEnd - zIn);
            }

            return SXRET_OK;
        }
        zEnd--;
        if (zEnd < zIn)
        {
            break;
        }
        if (zEnd[0] == c)
        {
            if (pPos)
            {
                *pPos = (sxu32)(zEnd - zIn);
            }

            return SXRET_OK;
        }
        zEnd--;
        if (zEnd < zIn)
        {
            break;
        }
        if (zEnd[0] == c)
        {
            if (pPos)
            {
                *pPos = (sxu32)(zEnd - zIn);
            }

            return SXRET_OK;
        }
        zEnd--;
        if (zEnd < zIn)
        {
            break;
        }
        if (zEnd[0] == c)
        {
            if (pPos)
            {
                *pPos = (sxu32)(zEnd - zIn);
            }

            return SXRET_OK;
        }
        zEnd--;
    }

    return SXERR_NOTFOUND;
}

#endif /* PH7_DISABLE_BUILTIN_FUNC */

PH7_PRIVATE sxi32 SyByteListFind(const char* zSrc, sxu32 nLen, const char* zList, sxu32* pFirstPos)
{
    const char* zIn = zSrc;
    const char* zPtr;
    sxi32 c;
    const char* zEnd = &zSrc[nLen];
    for (;;)
    {
        if (zIn >= zEnd)
        {
            break;
        }
        for (zPtr = zList; (c = zPtr[0]) != 0; zPtr++)
        {
            if (zIn[0] == c)
            {
                if (pFirstPos)
                {
                    *pFirstPos = (sxu32)(zIn - zSrc);
                }

                return SXRET_OK;
            }
        }
        zIn++;
        if (zIn >= zEnd)
        {
            break;
        }
        for (zPtr = zList; (c = zPtr[0]) != 0; zPtr++)
        {
            if (zIn[0] == c)
            {
                if (pFirstPos)
                {
                    *pFirstPos = (sxu32)(zIn - zSrc);
                }

                return SXRET_OK;
            }
        }
        zIn++;
        if (zIn >= zEnd)
        {
            break;
        }
        for (zPtr = zList; (c = zPtr[0]) != 0; zPtr++)
        {
            if (zIn[0] == c)
            {
                if (pFirstPos)
                {
                    *pFirstPos = (sxu32)(zIn - zSrc);
                }

                return SXRET_OK;
            }
        }
        zIn++;
        if (zIn >= zEnd)
        {
            break;
        }
        for (zPtr = zList; (c = zPtr[0]) != 0; zPtr++)
        {
            if (zIn[0] == c)
            {
                if (pFirstPos)
                {
                    *pFirstPos = (sxu32)(zIn - zSrc);
                }

                return SXRET_OK;
            }
        }
        zIn++;
    }

    return SXERR_NOTFOUND;
}

#ifndef PH7_DISABLE_BUILTIN_FUNC

PH7_PRIVATE sxi32 SyStrncmp(const char* zLeft, const char* zRight, sxu32 nLen)
{
    const unsigned char* zP = (const unsigned char*)zLeft;
    const unsigned char* zQ = (const unsigned char*)zRight;

    if (SX_EMPTY_STR(zP) || SX_EMPTY_STR(zQ))
    {
        return SX_EMPTY_STR(zP) ? (SX_EMPTY_STR(zQ) ? 0 : -1) : 1;
    }
    if (nLen <= 0)
    {
        return 0;
    }
    for (;;)
    {
        if (nLen <= 0)
        {
            return 0;
        }
        if (zP[0] == 0 || zQ[0] == 0 || zP[0] != zQ[0])
        {
            break;
        }
        zP++;
        zQ++;
        nLen--;
        if (nLen <= 0)
        {
            return 0;
        }
        if (zP[0] == 0 || zQ[0] == 0 || zP[0] != zQ[0])
        {
            break;
        }
        zP++;
        zQ++;
        nLen--;
        if (nLen <= 0)
        {
            return 0;
        }
        if (zP[0] == 0 || zQ[0] == 0 || zP[0] != zQ[0])
        {
            break;
        }
        zP++;
        zQ++;
        nLen--;
        if (nLen <= 0)
        {
            return 0;
        }
        if (zP[0] == 0 || zQ[0] == 0 || zP[0] != zQ[0])
        {
            break;
        }
        zP++;
        zQ++;
        nLen--;
    }

    return (sxi32)(zP[0] - zQ[0]);
}

#endif

PH7_PRIVATE sxi32 SyStrnicmp(const char* zLeft, const char* zRight, sxu32 SLen)
{
    register unsigned char* p = (unsigned char*)zLeft;
    register unsigned char* q = (unsigned char*)zRight;

    if (SX_EMPTY_STR(p) || SX_EMPTY_STR(q))
    {
        return SX_EMPTY_STR(p) ? SX_EMPTY_STR(q) ? 0 : -1 : 1;
    }
    for (;;)
    {
        if (SLen == 0)
        {
            return 0;
        }
        if (!*p || !*q || SyCharToLower(*p) != SyCharToLower(*q))
        {
            break;
        }
        p++;
        q++;
        SLen--;
        if (SLen == 0)
        {
            return 0;
        }
        if (!*p || !*q || SyCharToLower(*p) != SyCharToLower(*q))
        {
            break;
        }
        p++;
        q++;
        SLen--;
        if (SLen == 0)
        {
            return 0;
        }
        if (!*p || !*q || SyCharToLower(*p) != SyCharToLower(*q))
        {
            break;
        }
        p++;
        q++;
        SLen--;
        if (SLen == 0)
        {
            return 0;
        }
        if (!*p || !*q || SyCharToLower(*p) != SyCharToLower(*q))
        {
            break;
        }
        p++;
        q++;
        SLen--;
    }

    return (sxi32)(SyCharToLower(p[0]) - SyCharToLower(q[0]));
}

PH7_PRIVATE sxi32 SyStrnmicmp(const void* pLeft, const void* pRight, sxu32 SLen)
{
    return SyStrnicmp((const char*)pLeft, (const char*)pRight, SLen);
}

static sxu32 Systrcpy(char* zDest, sxu32 nDestLen, const char* zSrc, sxu32 nLen)
{
    unsigned char* zBuf = (unsigned char*)zDest;

#if defined(UNTRUST)
    if (zSrc == (const char*)zDest)
    {
        return 0;
    }
#endif
    if (nLen <= 0)
    {
        nLen = SyStrlen(zSrc);
    }
    // Reserve a room for the null terminator.
    unsigned char* zEnd = &zBuf[nDestLen - 1];
    for (;;)
    {
        if (zBuf >= zEnd || nLen == 0)
        {
            break;
        }
        unsigned char* zIn = (unsigned char*)zSrc;
        zBuf[0] = zIn[0];
        zIn++;
        zBuf++;
        nLen--;
        if (zBuf >= zEnd || nLen == 0)
        {
            break;
        }
        zBuf[0] = zIn[0];
        zIn++;
        zBuf++;
        nLen--;
        if (zBuf >= zEnd || nLen == 0)
        {
            break;
        }
        zBuf[0] = zIn[0];
        zIn++;
        zBuf++;
        nLen--;
        if (zBuf >= zEnd || nLen == 0)
        {
            break;
        }
        zBuf[0] = zIn[0];
        zIn++;
        zBuf++;
        nLen--;
    }
    zBuf[0] = 0;

    return (sxu32)(zBuf - (unsigned char*)zDest);
}

/* SyRunTimeApi:sxmem.c */
PH7_PRIVATE void SyZero(void* pSrc, sxu32 nSize)
{
    register unsigned char* zSrc = (unsigned char*)pSrc;

#if defined(UNTRUST)
    if (zSrc == 0 || nSize <= 0)
    {
        return;
    }
#endif

    unsigned char* zEnd = &zSrc[nSize];
    for (;;)
    {
        if (zSrc >= zEnd)
        {
            break;
        }
        zSrc[0] = 0;
        zSrc++;
        if (zSrc >= zEnd)
        {
            break;
        }
        zSrc[0] = 0;
        zSrc++;
        if (zSrc >= zEnd)
        {
            break;
        }
        zSrc[0] = 0;
        zSrc++;
        if (zSrc >= zEnd)
        {
            break;
        }
        zSrc[0] = 0;
        zSrc++;
    }
}

PH7_PRIVATE sxi32 SyMemcmp(const void* pB1, const void* pB2, sxu32 nSize)
{
    if (nSize <= 0)
    {
        return 0;
    }
    if (pB1 == 0 || pB2 == 0)
    {
        return pB1 != 0 ? 1 : (pB2 == 0 ? 0 : -1);
    }
    sxi32 rc;
    SX_MACRO_FAST_CMP(pB1, pB2, nSize, rc);

    return rc;
}

PH7_PRIVATE sxu32 SyMemcpy(const void* pSrc, void* pDest, sxu32 nLen)
{
#if defined(UNTRUST)
    if (pSrc == 0 || pDest == 0)
    {
        return 0;
    }
#endif

    if (pSrc == (const void*)pDest)
    {
        return nLen;
    }
    SX_MACRO_FAST_MEMCPY(pSrc, pDest, nLen);

    return nLen;
}

static void* MemOSAlloc(sxu32 nBytes)
{
    sxu32* pChunk;
    pChunk = (sxu32*)
        SyOSHeapAlloc(nBytes + sizeof(sxu32));
    if (pChunk == 0)
    {
        return 0;
    }
    pChunk[0] = nBytes;

    return (void*)&pChunk[1];
}

static void* MemOSRealloc(void* pOld, sxu32 nBytes)
{
    sxu32* pOldChunk;
    sxu32* pChunk;
    pOldChunk = (sxu32*)(((char*)pOld) - sizeof(sxu32));
    if (pOldChunk[0] >= nBytes)
    {
        return pOld;
    }
    pChunk = (sxu32*)
        SyOSHeapRealloc(pOldChunk, nBytes + sizeof(sxu32));
    if (pChunk == 0)
    {
        return 0;
    }
    pChunk[0] = nBytes;

    return (void*)&pChunk[1];
}

static void MemOSFree(void* pBlock)
{
    void* pChunk;
    pChunk = (void*)(((char*)pBlock) - sizeof(sxu32));
    SyOSHeapFree(pChunk);
}

static sxu32 MemOSChunkSize(void* pBlock)
{
    sxu32* pChunk;
    pChunk = (sxu32*)(((char*)pBlock) - sizeof(sxu32));

    return pChunk[0];
}

/* Export OS allocation methods */
static const SyMemMethods sOSAllocMethods = {
    MemOSAlloc,
    MemOSRealloc,
    MemOSFree,
    MemOSChunkSize,
    0,
    0,
    0
};

static void* MemBackendAlloc(SyMemBackend* pBackend, sxu32 nByte)
{
    // Append an extra block so we can tracks allocated chunks and avoid memory leaks.
    nByte += sizeof(SyMemBlock);
    SyMemBlock* pBlock;
    sxi32 nRetry = 0;
    for (;;)
    {
        pBlock = (SyMemBlock*)pBackend->pMethods->xAlloc(nByte);
        if (pBlock != 0 || pBackend->xMemError == 0 || nRetry > SXMEM_BACKEND_RETRY
            || SXERR_RETRY != pBackend->xMemError(pBackend->pUserData))
        {
            break;
        }
        nRetry++;
    }
    if (pBlock == 0)
    {
        return 0;
    }
    pBlock->pNext = pBlock->pPrev = 0;
    /* Link to the list of already tracked blocks */
    MACRO_LD_PUSH(pBackend->pBlocks, pBlock);

#if defined(UNTRUST)
    pBlock->nGuard = SXMEM_BACKEND_MAGIC;
#endif
    pBackend->nBlock++;

    return (void*)&pBlock[1];
}

PH7_PRIVATE void* SyMemBackendAlloc(SyMemBackend* pBackend, sxu32 nByte)
{
#if defined(UNTRUST)
    if (SXMEM_BACKEND_CORRUPT(pBackend))
    {
        return 0;
    }
#endif

    if (pBackend->pMutexMethods)
    {
        SyMutexEnter(pBackend->pMutexMethods, pBackend->pMutex);
    }
    void* pChunk = MemBackendAlloc(&(*pBackend), nByte);
    if (pBackend->pMutexMethods)
    {
        SyMutexLeave(pBackend->pMutexMethods, pBackend->pMutex);
    }

    return pChunk;
}

static void* MemBackendRealloc(SyMemBackend* pBackend, void* pOld, sxu32 nByte)
{
    if (pOld == 0)
    {
        return MemBackendAlloc(&(*pBackend), nByte);
    }
    SyMemBlock* pBlock = (SyMemBlock*)(((char*)pOld) - sizeof(SyMemBlock));
#if defined(UNTRUST)
    if (pBlock->nGuard != SXMEM_BACKEND_MAGIC)
    {
        return 0;
    }
#endif
    nByte += sizeof(SyMemBlock);
    SyMemBlock* pPrev = pBlock->pPrev;
    SyMemBlock* pNext = pBlock->pNext;
    SyMemBlock* pNew;
    sxu32 nRetry = 0;
    for (;;)
    {
        pNew = (SyMemBlock*)pBackend->pMethods->xRealloc(pBlock, nByte);
        if (pNew != 0 || pBackend->xMemError == 0 || nRetry > SXMEM_BACKEND_RETRY ||
            SXERR_RETRY != pBackend->xMemError(pBackend->pUserData))
        {
            break;
        }
        nRetry++;
    }
    if (pNew == 0)
    {
        return 0;
    }
    if (pNew != pBlock)
    {
        if (pPrev == 0)
        {
            pBackend->pBlocks = pNew;
        }
        else
        {
            pPrev->pNext = pNew;
        }
        if (pNext)
        {
            pNext->pPrev = pNew;
        }
#if defined(UNTRUST)
        pNew->nGuard = SXMEM_BACKEND_MAGIC;
#endif
    }

    return (void*)&pNew[1];
}

PH7_PRIVATE void* SyMemBackendRealloc(SyMemBackend* pBackend, void* pOld, sxu32 nByte)
{
#if defined(UNTRUST)
    if (SXMEM_BACKEND_CORRUPT(pBackend))
    {
        return 0;
    }
#endif

    if (pBackend->pMutexMethods)
    {
        SyMutexEnter(pBackend->pMutexMethods, pBackend->pMutex);
    }
    void* pChunk = MemBackendRealloc(&(*pBackend), pOld, nByte);
    if (pBackend->pMutexMethods)
    {
        SyMutexLeave(pBackend->pMutexMethods, pBackend->pMutex);
    }

    return pChunk;
}

static sxi32 MemBackendFree(SyMemBackend* pBackend, void* pChunk)
{
    SyMemBlock* pBlock = (SyMemBlock*)(((char*)pChunk) - sizeof(SyMemBlock));

#if defined(UNTRUST)
    if (pBlock->nGuard != SXMEM_BACKEND_MAGIC)
    {
        return SXERR_CORRUPT;
    }
#endif

    // Unlink from the list of active blocks.
    if (pBackend->nBlock > 0)
    {
        // Release the block.
#if defined(UNTRUST)
        // Mark as stale block.
        pBlock->nGuard = 0x635B;
#endif
        MACRO_LD_REMOVE(pBackend->pBlocks, pBlock);
        pBackend->nBlock--;
        pBackend->pMethods->xFree(pBlock);
    }

    return SXRET_OK;
}

PH7_PRIVATE sxi32 SyMemBackendFree(SyMemBackend* pBackend, void* pChunk)
{
#if defined(UNTRUST)
    if (SXMEM_BACKEND_CORRUPT(pBackend))
    {
        return SXERR_CORRUPT;
    }
#endif

    if (pChunk == 0)
    {
        return SXRET_OK;
    }
    if (pBackend->pMutexMethods)
    {
        SyMutexEnter(pBackend->pMutexMethods, pBackend->pMutex);
    }
    sxi32 rc = MemBackendFree(&(*pBackend), pChunk);
    if (pBackend->pMutexMethods)
    {
        SyMutexLeave(pBackend->pMutexMethods, pBackend->pMutex);
    }

    return rc;
}

#if defined(PH7_ENABLE_THREADS)
PH7_PRIVATE sxi32 SyMemBackendMakeThreadSafe(SyMemBackend* pBackend, const SyMutexMethods* pMethods)
{
    SyMutex* pMutex;
#if defined(UNTRUST)
    if (SXMEM_BACKEND_CORRUPT(pBackend) || pMethods == 0 || pMethods->xNew == 0)
    {
        return SXERR_CORRUPT;
    }
#endif
    pMutex = pMethods->xNew(SXMUTEX_TYPE_FAST);
    if (pMutex == 0)
    {
        return SXERR_OS;
    }
    /* Attach the mutex to the memory backend */
    pBackend->pMutex = pMutex;
    pBackend->pMutexMethods = pMethods;
    return SXRET_OK;
}

PH7_PRIVATE sxi32 SyMemBackendDisbaleMutexing(SyMemBackend* pBackend)
{
#if defined(UNTRUST)
    if (SXMEM_BACKEND_CORRUPT(pBackend))
    {
        return SXERR_CORRUPT;
    }
#endif
    if (pBackend->pMutex == 0)
    {
        /* There is no mutex subsystem at all */
        return SXRET_OK;
    }
    SyMutexRelease(pBackend->pMutexMethods, pBackend->pMutex);
    pBackend->pMutexMethods = 0;
    pBackend->pMutex = 0;
    return SXRET_OK;
}

#endif

// Memory pool allocator
#define SXMEM_POOL_MAGIC        0xDEADu
#define SXMEM_POOL_MAXALLOC     1u << (SXMEM_POOL_NBUCKETS + SXMEM_POOL_INCR)
#define SXMEM_POOL_MINALLOC     1u << (SXMEM_POOL_INCR)

static sxi32 MemPoolBucketAlloc(SyMemBackend* pBackend, sxu32 nBucket)
{
    /* Allocate one big block first */
    char* zBucket = (char*)MemBackendAlloc(&(*pBackend), SXMEM_POOL_MAXALLOC);
    if (zBucket == 0)
    {
        return SXERR_MEM;
    }
    char* zBucketEnd = &zBucket[SXMEM_POOL_MAXALLOC];
    /* Divide the big block into mini bucket pool */
    sxu32 nBucketSize = 1u << (nBucket + SXMEM_POOL_INCR);
    SyMemHeader* pHeader;
    pBackend->apPool[nBucket] = pHeader = (SyMemHeader*)zBucket;
    for (;;)
    {
        if (&zBucket[nBucketSize] >= zBucketEnd)
        {
            break;
        }
        pHeader->pNext = (SyMemHeader*)&zBucket[nBucketSize];
        /* Advance the cursor to the next available chunk */
        pHeader = pHeader->pNext;
        zBucket += nBucketSize;
    }
    pHeader->pNext = 0;

    return SXRET_OK;
}

static void* MemBackendPoolAlloc(SyMemBackend* pBackend, sxu32 nByte)
{
    SyMemHeader* pBucket;
    if (nByte + sizeof(SyMemHeader) >= SXMEM_POOL_MAXALLOC)
    {
        /* Allocate a big chunk directly */
        pBucket = (SyMemHeader*)MemBackendAlloc(&(*pBackend), nByte + sizeof(SyMemHeader));
        if (pBucket == 0)
        {
            return 0;
        }
        /* Record as big block */
        pBucket->nBucket = (sxu32)(SXMEM_POOL_MAGIC << 16u) | SXU16_HIGH;
        return (void*)(pBucket + 1);
    }
    /* Locate the appropriate bucket */
    sxu32 nBucket = 0;
    sxu32 nBucketSize = SXMEM_POOL_MINALLOC;
    while (nByte + sizeof(SyMemHeader) > nBucketSize)
    {
        nBucketSize <<= 1u;
        nBucket++;
    }
    pBucket = pBackend->apPool[nBucket];
    if (pBucket == 0)
    {
        sxi32 rc = MemPoolBucketAlloc(&(*pBackend), nBucket);
        if (rc != SXRET_OK)
        {
            return 0;
        }
        pBucket = pBackend->apPool[nBucket];
    }
    /* Remove from the free list */
    SyMemHeader* pNext = pBucket->pNext;
    pBackend->apPool[nBucket] = pNext;
    /* Record bucket&magic number */
    pBucket->nBucket = (SXMEM_POOL_MAGIC << 16u) | nBucket;

    return (void*)&pBucket[1];
}

PH7_PRIVATE void* SyMemBackendPoolAlloc(SyMemBackend* pBackend, sxu32 nByte)
{
#if defined(UNTRUST)
    if (SXMEM_BACKEND_CORRUPT(pBackend))
    {
        return 0;
    }
#endif
    if (pBackend->pMutexMethods)
    {
        SyMutexEnter(pBackend->pMutexMethods, pBackend->pMutex);
    }
    void* pChunk = MemBackendPoolAlloc(&(*pBackend), nByte);
    if (pBackend->pMutexMethods)
    {
        SyMutexLeave(pBackend->pMutexMethods, pBackend->pMutex);
    }

    return pChunk;
}

static sxi32 MemBackendPoolFree(SyMemBackend* pBackend, void* pChunk)
{
    // Get the corresponding bucket.
    SyMemHeader* pHeader = (SyMemHeader*)(((char*)pChunk) - sizeof(SyMemHeader));
    // Sanity check to avoid misuse.
    if ((pHeader->nBucket >> 16u) != SXMEM_POOL_MAGIC)
    {
        return SXERR_CORRUPT;
    }
    sxu32 nBucket = pHeader->nBucket & 0xFFFFu;
    if (nBucket == SXU16_HIGH)
    {
        // Free the big block.
        MemBackendFree(&(*pBackend), pHeader);
    }
    else
    {
        // Return to the free list.
        pHeader->pNext = pBackend->apPool[nBucket & 0x0Fu];
        pBackend->apPool[nBucket & 0x0Fu] = pHeader;
    }

    return SXRET_OK;
}

PH7_PRIVATE sxi32 SyMemBackendPoolFree(SyMemBackend* pBackend, void* pChunk)
{
#if defined(UNTRUST)
    if (SXMEM_BACKEND_CORRUPT(pBackend) || pChunk == 0)
    {
        return SXERR_CORRUPT;
    }
#endif

    if (pBackend->pMutexMethods)
    {
        SyMutexEnter(pBackend->pMutexMethods, pBackend->pMutex);
    }
    sxi32 rc = MemBackendPoolFree(&(*pBackend), pChunk);
    if (pBackend->pMutexMethods)
    {
        SyMutexLeave(pBackend->pMutexMethods, pBackend->pMutex);
    }

    return rc;
}

#if 0

static void* MemBackendPoolRealloc(SyMemBackend* pBackend, void* pOld, sxu32 nByte)
{
    sxu32 nBucket, nBucketSize;
    SyMemHeader* pHeader;
    void* pNew;

    if (pOld == 0)
    {
        /* Allocate a new pool */
        pNew = MemBackendPoolAlloc(&(*pBackend), nByte);
        return pNew;
    }
    /* Get the corresponding bucket */
    pHeader = (SyMemHeader*)(((char*)pOld) - sizeof(SyMemHeader));
    /* Sanity check to avoid misuse */
    if ((pHeader->nBucket >> 16) != SXMEM_POOL_MAGIC)
    {
        return 0;
    }
    nBucket = pHeader->nBucket & 0xFFFF;
    if (nBucket == SXU16_HIGH)
    {
        /* Big block */
        return MemBackendRealloc(&(*pBackend), pHeader, nByte);
    }
    nBucketSize = 1 << (nBucket + SXMEM_POOL_INCR);
    if (nBucketSize >= nByte + sizeof(SyMemHeader))
    {
        /* The old bucket can honor the requested size */
        return pOld;
    }
    /* Allocate a new pool */
    pNew = MemBackendPoolAlloc(&(*pBackend), nByte);
    if (pNew == 0)
    {
        return 0;
    }
    /* Copy the old data into the new block */
    SyMemcpy(pOld, pNew, nBucketSize);
    /* Free the stale block */
    MemBackendPoolFree(&(*pBackend), pOld);
    return pNew;
}

PH7_PRIVATE void* SyMemBackendPoolRealloc(SyMemBackend* pBackend, void* pOld, sxu32 nByte)
{
    void* pChunk;
#if defined(UNTRUST)
    if (SXMEM_BACKEND_CORRUPT(pBackend))
    {
        return 0;
    }
#endif
    if (pBackend->pMutexMethods)
    {
        SyMutexEnter(pBackend->pMutexMethods, pBackend->pMutex);
    }
    pChunk = MemBackendPoolRealloc(&(*pBackend), pOld, nByte);
    if (pBackend->pMutexMethods)
    {
        SyMutexLeave(pBackend->pMutexMethods, pBackend->pMutex);
    }
    return pChunk;
}

#endif

PH7_PRIVATE sxi32 SyMemBackendInit(
    SyMemBackend* pBackend,
    ProcMemError xMemErr,
    void* pUserData)
{
#if defined(UNTRUST)
    if (pBackend == 0)
    {
        return SXERR_EMPTY;
    }
#endif

// Zero the allocator first */
    SyZero(&(*pBackend), sizeof(SyMemBackend));
    pBackend->xMemError = xMemErr;
    pBackend->pUserData = pUserData;
    // Switch to the OS memory allocator */
    pBackend->pMethods = &sOSAllocMethods;
    if (pBackend->pMethods->xInit)
    {
        // Initialize the backend.
        if (SXRET_OK != pBackend->pMethods->
            xInit(pBackend
                      ->pMethods->pUserData))
        {
            return
                SXERR_ABORT;
        }
    }

#if defined(UNTRUST)
    pBackend->nMagic = SXMEM_BACKEND_MAGIC;
#endif

    return SXRET_OK;
}

PH7_PRIVATE sxi32 SyMemBackendInitFromOthers(
    SyMemBackend* pBackend,
    const SyMemMethods* pMethods,
    ProcMemError xMemErr,
    void* pUserData)
{
#if defined(UNTRUST)
    if (pBackend == 0 || pMethods == 0)
    {
        return SXERR_EMPTY;
    }
#endif

    if (pMethods->xAlloc == 0 ||
        pMethods->xRealloc == 0 ||
        pMethods->xFree == 0 ||
        pMethods->xChunkSize == 0)
    {
        // mandatory methods are missing.
        return SXERR_INVALID;
    }
    // Zero the allocator first.
    SyZero(&(*pBackend), sizeof(SyMemBackend));
    pBackend->xMemError = xMemErr;
    pBackend->pUserData = pUserData;
    // Switch to the host application memory allocator.
    pBackend->pMethods = pMethods;
    if (pBackend->pMethods->xInit)
    {
        // Initialize the backend.
        if (SXRET_OK != pBackend->pMethods->
            xInit(pBackend->pMethods->pUserData))
        {
            return SXERR_ABORT;
        }
    }

#if defined(UNTRUST)
    pBackend->nMagic = SXMEM_BACKEND_MAGIC;
#endif

    return SXRET_OK;
}

PH7_PRIVATE sxi32 SyMemBackendInitFromParent(SyMemBackend* pBackend, SyMemBackend* pParent)
{
#if defined(UNTRUST)
    if (pBackend == 0 || SXMEM_BACKEND_CORRUPT(pParent))
    {
        return SXERR_CORRUPT;
    }
#endif

    // Zero the allocator first.
    SyZero(&(*pBackend), sizeof(SyMemBackend));
    pBackend->pMethods = pParent->pMethods;
    pBackend->xMemError = pParent->xMemError;
    pBackend->pUserData = pParent->pUserData;
    sxu8 bInheritMutex = pParent->pMutexMethods ? TRUE : FALSE;
    if (bInheritMutex)
    {
        pBackend->pMutexMethods = pParent->pMutexMethods;
        // Create a private mutex
        pBackend->pMutex = pBackend->pMutexMethods->xNew(SXMUTEX_TYPE_FAST);
        if (pBackend->pMutex == 0)
        {
            return SXERR_OS;
        }
    }

#if defined(UNTRUST)
    pBackend->nMagic = SXMEM_BACKEND_MAGIC;
#endif

    return SXRET_OK;
}

static sxi32 MemBackendRelease(SyMemBackend* pBackend)
{
    SyMemBlock* pBlock = pBackend->pBlocks;
    for (;;)
    {
        if (pBackend->nBlock == 0)
        {
            break;
        }
        SyMemBlock* pNext = pBlock->pNext;
        pBackend->pMethods->xFree(pBlock);
        pBlock = pNext;
        pBackend->nBlock--;
        // LOOP ONE
        if (pBackend->nBlock == 0)
        {
            break;
        }
        pNext = pBlock->pNext;
        pBackend->pMethods->xFree(pBlock);
        pBlock = pNext;
        pBackend->nBlock--;
        // LOOP TWO
        if (pBackend->nBlock == 0)
        {
            break;
        }
        pNext = pBlock->pNext;
        pBackend->pMethods->xFree(pBlock);
        pBlock = pNext;
        pBackend->nBlock--;
        // LOOP THREE
        if (pBackend->nBlock == 0)
        {
            break;
        }
        pNext = pBlock->pNext;
        pBackend->pMethods->xFree(pBlock);
        pBlock = pNext;
        pBackend->nBlock--;
        // LOOP FOUR
    }
    if (pBackend->pMethods->xRelease)
    {
        pBackend->pMethods->xRelease(pBackend->pMethods->pUserData);
    }
    pBackend->pMethods = 0;
    pBackend->pBlocks = 0;

#if defined(UNTRUST)
    pBackend->nMagic = 0x2626;
#endif

    return SXRET_OK;
}

PH7_PRIVATE sxi32 SyMemBackendRelease(SyMemBackend* pBackend)
{
#if defined(UNTRUST)
    if (SXMEM_BACKEND_CORRUPT(pBackend))
    {
        return SXERR_INVALID;
    }
#endif

    if (pBackend->pMutexMethods)
    {
        SyMutexEnter(pBackend->pMutexMethods, pBackend->pMutex);
    }
    sxi32 rc = MemBackendRelease(&(*pBackend));
    if (pBackend->pMutexMethods)
    {
        SyMutexLeave(pBackend->pMutexMethods, pBackend->pMutex);
        SyMutexRelease(pBackend->pMutexMethods, pBackend->pMutex);
    }

    return SXRET_OK;
}

PH7_PRIVATE void* SyMemBackendDup(SyMemBackend* pBackend, const void* pSrc, sxu32 nSize)
{
#if defined(UNTRUST)
    if (pSrc == 0 || nSize <= 0)
    {
        return 0;
    }
#endif

    void* pNew = SyMemBackendAlloc(&(*pBackend), nSize);
    if (pNew)
    {
        SyMemcpy(pSrc, pNew, nSize);
    }

    return pNew;
}

PH7_PRIVATE char* SyMemBackendStrDup(SyMemBackend* pBackend, const char* zSrc, sxu32 nSize)
{
    char* zDest = (char*)SyMemBackendAlloc(&(*pBackend), nSize + 1);
    if (zDest != NULL)
    {
        Systrcpy(zDest, nSize + 1, zSrc, nSize);
    }

    return zDest;
}

PH7_PRIVATE sxi32 SyBlobInitFromBuf(SyBlob* pBlob, void* pBuffer, sxu32 nSize)
{
#if defined(UNTRUST)
    if (pBlob == 0 || pBuffer == 0 || nSize < 1)
    {
        return SXERR_EMPTY;
    }
#endif

    pBlob->pBlob = pBuffer;
    pBlob->mByte = nSize;
    pBlob->nByte = 0;
    pBlob->pAllocator = 0;
    pBlob->nFlags = SXBLOB_LOCKED | SXBLOB_STATIC;

    return SXRET_OK;
}

PH7_PRIVATE sxi32 SyBlobInit(SyBlob* pBlob, SyMemBackend* pAllocator)
{
#if defined(UNTRUST)
    if (pBlob == 0)
    {
        return SXERR_EMPTY;
    }
#endif

    pBlob->pBlob = 0;
    pBlob->mByte = pBlob->nByte = 0;
    pBlob->pAllocator = &(*pAllocator);
    pBlob->nFlags = 0;

    return SXRET_OK;
}

PH7_PRIVATE sxi32 SyBlobReadOnly(SyBlob* pBlob, const void* pData, sxu32 nByte)
{
#if defined(UNTRUST)
    if (pBlob == 0)
    {
        return SXERR_EMPTY;
    }
#endif

    pBlob->pBlob = (void*)pData;
    pBlob->nByte = nByte;
    pBlob->mByte = 0;
    pBlob->nFlags |= SXBLOB_RDONLY;

    return SXRET_OK;
}

#ifndef SXBLOB_MIN_GROWTH
#define SXBLOB_MIN_GROWTH 16
#endif

static sxi32 BlobPrepareGrow(SyBlob* pBlob, sxu32* pByte)
{
    sxu32 nByte = *pByte;
    if (pBlob->nFlags & (SXBLOB_LOCKED | SXBLOB_STATIC))
    {
        if (SyBlobFreeSpace(pBlob) < nByte)
        {
            *pByte = SyBlobFreeSpace(pBlob);
            if ((*pByte) == 0)
            {
                return SXERR_SHORT;
            }
        }
        return SXRET_OK;
    }
    void* pNew;
    if (pBlob->nFlags & SXBLOB_RDONLY)
    {
        // Make a copy of the read-only item.
        if (pBlob->nByte > 0)
        {
            pNew = SyMemBackendDup(pBlob->pAllocator, pBlob->pBlob, pBlob->nByte);
            if (pNew == 0)
            {
                return SXERR_MEM;
            }
            pBlob->pBlob = pNew;
            pBlob->mByte = pBlob->nByte;
        }
        else
        {
            pBlob->pBlob = 0;
            pBlob->mByte = 0;
        }
        // Remove the read-only flag.
        pBlob->nFlags &= ~SXBLOB_RDONLY;
    }
    if (SyBlobFreeSpace(pBlob) >= nByte)
    {
        return SXRET_OK;
    }
    if (pBlob->mByte > 0)
    {
        nByte = nByte + pBlob->mByte * 2 + SXBLOB_MIN_GROWTH;
    }
    else if (nByte < SXBLOB_MIN_GROWTH)
    {
        nByte = SXBLOB_MIN_GROWTH;
    }
    pNew = SyMemBackendRealloc(pBlob->pAllocator, pBlob->pBlob, nByte);
    if (pNew == 0)
    {
        return SXERR_MEM;
    }
    pBlob->pBlob = pNew;
    pBlob->mByte = nByte;

    return SXRET_OK;
}

PH7_PRIVATE sxi32 SyBlobAppend(SyBlob* pBlob, const void* pData, sxu32 nSize)
{
    if (nSize < 1)
    {
        return SXRET_OK;
    }
    sxi32 rc = BlobPrepareGrow(&(*pBlob), &nSize);
    if (SXRET_OK != rc)
    {
        return rc;
    }
    if (pData != NULL)
    {
        sxu8* zBlob = (sxu8*)pBlob->pBlob;
        zBlob = &zBlob[pBlob->nByte];
        pBlob->nByte += nSize;
        SX_MACRO_FAST_MEMCPY(pData, zBlob, nSize);
    }

    return SXRET_OK;
}

PH7_PRIVATE sxi32 SyBlobNullAppend(SyBlob* pBlob)
{
    sxu32 n = pBlob->nByte;

    sxi32 rc = SyBlobAppend(&(*pBlob), (const void*)'\0', sizeof(char));
    if (rc == SXRET_OK)
    {
        pBlob->nByte = n;
    }

    return rc;
}

PH7_PRIVATE sxi32 SyBlobDup(SyBlob* pSrc, SyBlob* pDest)
{
#ifdef UNTRUST
    if (pSrc == 0 || pDest == 0)
    {
        return SXERR_EMPTY;
    }
#endif

    if (pSrc->nByte > 0)
    {
        return SyBlobAppend(&(*pDest), pSrc->pBlob, pSrc->nByte);
    }

    return SXRET_OK;
}

PH7_PRIVATE sxi32 SyBlobCmp(SyBlob* pLeft, SyBlob* pRight)
{
#ifdef UNTRUST
    if (pLeft == 0 || pRight == 0)
    {
        return pLeft ? 1 : -1;
    }
#endif

    if (pLeft->nByte != pRight->nByte)
    {
        // Length differ
        return pLeft->nByte - pRight->nByte;
    }
    if (pLeft->nByte == 0)
    {
        return 0;
    }

    // Perform a standard memcmp() operation
    return SyMemcmp(pLeft->pBlob, pRight->pBlob, pLeft->nByte);
}

PH7_PRIVATE sxi32 SyBlobReset(SyBlob* pBlob)
{
    pBlob->nByte = 0;

    if (pBlob->nFlags & SXBLOB_RDONLY)
    {
        pBlob->pBlob = 0;
        pBlob->mByte = 0;
        pBlob->nFlags &= ~SXBLOB_RDONLY;
    }

    return SXRET_OK;
}

PH7_PRIVATE sxi32 SyBlobRelease(SyBlob* pBlob)
{
    if ((pBlob->nFlags & (SXBLOB_STATIC | SXBLOB_RDONLY)) == 0 && pBlob->mByte > 0)
    {
        SyMemBackendFree(pBlob->pAllocator, pBlob->pBlob);
    }
    pBlob->pBlob = 0;
    pBlob->nByte = pBlob->mByte = 0;
    pBlob->nFlags = 0;

    return SXRET_OK;
}

#ifndef PH7_DISABLE_BUILTIN_FUNC

PH7_PRIVATE sxi32 SyBlobSearch(
    const void* pBlob,
    sxu32 nLen,
    const void* pPattern,
    sxu32 pLen,
    sxu32* pOfft)
{
    const char* zIn = (const char*)pBlob;
    if (pLen > nLen)
    {
        return SXERR_NOTFOUND;
    }
    const char* zEnd = &zIn[nLen - pLen];
    for (;;)
    {
        if (zIn > zEnd)
        {
            break;
        }
        sxi32 rc;
        SX_MACRO_FAST_CMP(zIn, pPattern, pLen, rc);
        if (rc == 0)
        {
            if (pOfft)
            {
                *pOfft = (sxu32)(zIn - (const char*)pBlob);
            }
            return SXRET_OK;
        }
        zIn++;
        if (zIn > zEnd)
        {
            break;
        }
        SX_MACRO_FAST_CMP(zIn, pPattern, pLen, rc);
        if (rc == 0)
        {
            if (pOfft)
            {
                *pOfft = (sxu32)(zIn - (const char*)pBlob);
            }
            return SXRET_OK;
        }
        zIn++;
        if (zIn > zEnd)
        {
            break;
        }
        SX_MACRO_FAST_CMP(zIn, pPattern, pLen, rc);
        if (rc == 0)
        {
            if (pOfft)
            {
                *pOfft = (sxu32)(zIn - (const char*)pBlob);
            }
            return SXRET_OK;
        }
        zIn++;
        if (zIn > zEnd)
        {
            break;
        }
        SX_MACRO_FAST_CMP(zIn, pPattern, pLen, rc);
        if (rc == 0)
        {
            if (pOfft)
            {
                *pOfft = (sxu32)(zIn - (const char*)pBlob);
            }
            return SXRET_OK;
        }
        zIn++;
    }

    return SXERR_NOTFOUND;
}

#endif /* PH7_DISABLE_BUILTIN_FUNC */

/* SyRunTimeApi:sxds.c */
PH7_PRIVATE sxi32 SySetInit(
    SySet* pSet,
    SyMemBackend* pAllocator,
    sxu32 ElemSize)
{
    pSet->nSize = 0;
    pSet->nUsed = 0;
    pSet->nCursor = 0;
    pSet->eSize = ElemSize;
    pSet->pAllocator = pAllocator;
    pSet->pBase = 0;
    pSet->pUserData = 0;

    return SXRET_OK;
}

PH7_PRIVATE sxi32 SySetPut(SySet* pSet, const void* pItem)
{
    if (pSet->nUsed >= pSet->nSize)
    {
        if (pSet->pAllocator == 0)
        {
            return SXERR_LOCKED;
        }
        if (pSet->nSize <= 0)
        {
            pSet->nSize = 4;
        }
        void* pNew = SyMemBackendRealloc(pSet->pAllocator, pSet->pBase, pSet->eSize * pSet->nSize * 2);
        if (pNew == 0)
        {
            return SXERR_MEM;
        }
        pSet->pBase = pNew;
        pSet->nSize <<= 1u;
    }
    unsigned char* zBase = (unsigned char*)pSet->pBase;
    SX_MACRO_FAST_MEMCPY(pItem, &zBase[pSet->nUsed * pSet->eSize], pSet->eSize);
    pSet->nUsed++;

    return SXRET_OK;
}

PH7_PRIVATE sxi32 SySetAlloc(SySet* pSet, sxi32 nItem)
{
    if (pSet->nSize > 0)
    {
        return SXERR_LOCKED;
    }
    if (nItem < 8)
    {
        nItem = 8;
    }
    pSet->pBase = SyMemBackendAlloc(pSet->pAllocator, pSet->eSize * nItem);
    if (pSet->pBase == 0)
    {
        return SXERR_MEM;
    }
    pSet->nSize = nItem;

    return SXRET_OK;
}

PH7_PRIVATE sxi32 SySetReset(SySet* pSet)
{
    pSet->nUsed = 0;
    pSet->nCursor = 0;

    return SXRET_OK;
}

PH7_PRIVATE sxi32 SySetResetCursor(SySet* pSet)
{
    pSet->nCursor = 0;

    return SXRET_OK;
}

PH7_PRIVATE sxi32 SySetGetNextEntry(SySet* pSet, void** ppEntry)
{
    register unsigned char* zSrc;
    if (pSet->nCursor >= pSet->nUsed)
    {
        /* Reset cursor */
        pSet->nCursor = 0;

        return SXERR_EOF;
    }
    zSrc = (unsigned char*)SySetBasePtr(pSet);
    if (ppEntry)
    {
        *ppEntry = (void*)&zSrc[pSet->nCursor * pSet->eSize];
    }
    pSet->nCursor++;

    return SXRET_OK;
}

#ifndef PH7_DISABLE_BUILTIN_FUNC

PH7_PRIVATE void* SySetPeekCurrentEntry(SySet* pSet)
{
    register unsigned char* zSrc;
    if (pSet->nCursor >= pSet->nUsed)
    {
        return 0;
    }
    zSrc = (unsigned char*)SySetBasePtr(pSet);

    return (void*)&zSrc[pSet->nCursor * pSet->eSize];
}

#endif /* PH7_DISABLE_BUILTIN_FUNC */

PH7_PRIVATE sxi32 SySetTruncate(SySet* pSet, sxu32 nNewSize)
{
    if (nNewSize < pSet->nUsed)
    {
        pSet->nUsed = nNewSize;
    }
    return SXRET_OK;
}

PH7_PRIVATE sxi32 SySetRelease(SySet* pSet)
{
    sxi32 rc = SXRET_OK;
    if (pSet->pAllocator && pSet->pBase)
    {
        rc = SyMemBackendFree(pSet->pAllocator, pSet->pBase);
    }
    pSet->pBase = 0;
    pSet->nUsed = 0;
    pSet->nCursor = 0;

    return rc;
}

PH7_PRIVATE void* SySetPeek(SySet* pSet)
{
    if (pSet->nUsed <= 0)
    {
        return 0;
    }
    const char* zBase = (const char*)pSet->pBase;

    return (void*)&zBase[(pSet->nUsed - 1) * pSet->eSize];
}

PH7_PRIVATE void* SySetPop(SySet* pSet)
{
    if (pSet->nUsed <= 0)
    {
        return 0;
    }
    const char* zBase = (const char*)pSet->pBase;
    pSet->nUsed--;

    return (void*)&zBase[pSet->nUsed * pSet->eSize];
}

PH7_PRIVATE void* SySetAt(SySet* pSet, sxu32 nIdx)
{
    if (nIdx >= pSet->nUsed)
    {
        /* Out of range */
        return 0;
    }
    const char* zBase = (const char*)pSet->pBase;

    return (void*)&zBase[nIdx * pSet->eSize];
}

/* Private hash entry */
struct SyHashEntry_Pr
{
    const void* pKey; /* Hash key */
    sxu32 nKeyLen;    /* Key length */
    void* pUserData;  /* User private data */
    /* Private fields */
    sxu32 nHash;
    SyHash* pHash;
    SyHashEntry_Pr* pNext, * pPrev; /* Next and previous entry in the list */
    SyHashEntry_Pr* pNextCollide, * pPrevCollide; /* Collision list */
};
#define INVALID_HASH(H) ((H)->apBucket == 0)

/* Forward declarartion */
static sxu32 SyBinHash(const void* pSrc, sxu32 nLen);

PH7_PRIVATE sxi32 SyHashInit(
    SyHash* pHash,
    SyMemBackend* pAllocator,
    ProcHash xHash,
    ProcCmp xCmp)
{
#if defined(UNTRUST)
    if (pHash == 0)
    {
        return SXERR_EMPTY;
    }
#endif

    // Allocate a new table
    SyHashEntry_Pr** apNew = (SyHashEntry_Pr**)SyMemBackendAlloc(
        &(*pAllocator),
        sizeof(SyHashEntry_Pr*) * SXHASH_BUCKET_SIZE);
    if (apNew == 0)
    {
        return SXERR_MEM;
    }
    SyZero((void*)apNew, sizeof(SyHashEntry_Pr*) * SXHASH_BUCKET_SIZE);
    pHash->pAllocator = &(*pAllocator);
    pHash->xHash = xHash ? xHash : SyBinHash;
    pHash->xCmp = xCmp ? xCmp : SyMemcmp;
    pHash->pCurrent = pHash->pList = 0;
    pHash->nEntry = 0;
    pHash->apBucket = apNew;
    pHash->nBucketSize = SXHASH_BUCKET_SIZE;

    return SXRET_OK;
}

PH7_PRIVATE sxi32 SyHashRelease(SyHash* pHash)
{
#if defined(UNTRUST)
    if (INVALID_HASH(pHash))
    {
        return SXERR_EMPTY;
    }
#endif

    SyHashEntry_Pr* pEntry = pHash->pList;

    for (;;)
    {
        if (pHash->nEntry == 0)
        {
            break;
        }
        SyHashEntry_Pr* pNext = pEntry->pNext;
        SyMemBackendPoolFree(pHash->pAllocator, pEntry);
        pEntry = pNext;
        pHash->nEntry--;
    }
    if (pHash->apBucket)
    {
        SyMemBackendFree(pHash->pAllocator, (void*)pHash->apBucket);
    }
    pHash->apBucket = 0;
    pHash->nBucketSize = 0;
    pHash->pAllocator = 0;

    return SXRET_OK;
}

static SyHashEntry_Pr* HashGetEntry(SyHash* pHash, const void* pKey, sxu32 nKeyLen)
{
    sxu32 nHash = pHash->xHash(pKey, nKeyLen);
    SyHashEntry_Pr* pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];

    for (;;)
    {
        if (pEntry == 0)
        {
            break;
        }
        if (pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&
            pHash->xCmp(pEntry->pKey, pKey, nKeyLen) == 0)
        {
            return pEntry;
        }
        pEntry = pEntry->pNextCollide;
    }

    /* Entry not found */
    return 0;
}

PH7_PRIVATE SyHashEntry* SyHashGet(
    SyHash* pHash,
    const void* pKey,
    sxu32 nKeyLen)
{
#if defined(UNTRUST)
    if (INVALID_HASH(pHash))
    {
        return 0;
    }
#endif

    if (pHash->nEntry < 1 || nKeyLen < 1)
    {
        /* Don't bother hashing,return immediately */
        return 0;
    }
    SyHashEntry_Pr* pEntry = HashGetEntry(&(*pHash), pKey, nKeyLen);
    if (pEntry == 0)
    {
        return 0;
    }

    return (SyHashEntry*)pEntry;
}

static sxi32 HashDeleteEntry(SyHash* pHash, SyHashEntry_Pr* pEntry, void** ppUserData)
{
    if (pEntry->pPrevCollide == 0)
    {
        pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;
    }
    else
    {
        pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;
    }
    if (pEntry->pNextCollide != NULL)
    {
        pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;
    }
    MACRO_LD_REMOVE(pHash->pList, pEntry);
    pHash->nEntry--;
    if (ppUserData)
    {
        /* Write a pointer to the user data */
        *ppUserData = pEntry->pUserData;
    }

    /* Release the entry */
    return SyMemBackendPoolFree(pHash->pAllocator, pEntry);
}

PH7_PRIVATE sxi32 SyHashDeleteEntry(
    SyHash* pHash,
    const void* pKey,
    sxu32 nKeyLen,
    void** ppUserData)
{
#if defined(UNTRUST)
    if (INVALID_HASH(pHash))
    {
        return SXERR_CORRUPT;
    }
#endif

    SyHashEntry_Pr* pEntry = HashGetEntry(&(*pHash), pKey, nKeyLen);
    if (pEntry == 0)
    {
        return SXERR_NOTFOUND;
    }

    return HashDeleteEntry(&(*pHash), pEntry, ppUserData);
}

PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry* pEntry)
{
    SyHashEntry_Pr* pPtr = (SyHashEntry_Pr*)pEntry;

#if defined(UNTRUST)
    if (pPtr == 0 || INVALID_HASH(pPtr->pHash))
    {
        return SXERR_CORRUPT;
    }
#endif

    return HashDeleteEntry(pPtr->pHash, pPtr, 0);
}

PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash* pHash)
{
#if defined(UNTRUST)
    if (INVALID_HASH(pHash))
    {
        return SXERR_CORRUPT;
    }
#endif

    pHash->pCurrent = pHash->pList;

    return SXRET_OK;
}

PH7_PRIVATE SyHashEntry* SyHashGetNextEntry(SyHash* pHash)
{
#if defined(UNTRUST)
    if (INVALID_HASH(pHash))
    {
        return 0;
    }
#endif

    if (pHash->pCurrent == 0 || pHash->nEntry <= 0)
    {
        pHash->pCurrent = pHash->pList;

        return 0;
    }

    SyHashEntry_Pr* pEntry = pHash->pCurrent;
    /* Advance the cursor */
    pHash->pCurrent = pEntry->pNext;

    /* Return the current entry */
    return (SyHashEntry*)pEntry;
}

PH7_PRIVATE sxi32 SyHashForEach(
    SyHash* pHash,
    sxi32 (* xStep)(SyHashEntry*, void*),
    void* pUserData)
{
#if defined(UNTRUST)
    if (INVALID_HASH(pHash) || xStep == 0)
    {
        return 0;
    }
#endif

    SyHashEntry_Pr* pEntry = pHash->pList;
    for (sxu32 n = 0; n < pHash->nEntry; n++)
    {
        /* Invoke the callback */
        sxi32 rc = xStep((SyHashEntry*)pEntry, pUserData);
        if (rc != SXRET_OK)
        {
            return rc;
        }
        /* Point to the next entry */
        pEntry = pEntry->pNext;
    }

    return SXRET_OK;
}

static sxi32 HashGrowTable(SyHash* pHash)
{
    sxu32 nNewSize = pHash->nBucketSize * 2;
    SyHashEntry_Pr* pEntry;
    SyHashEntry_Pr** apNew;
    sxu32 n, iBucket;

    /* Allocate a new larger table */
    apNew = (SyHashEntry_Pr**)SyMemBackendAlloc(pHash->pAllocator, nNewSize * sizeof(SyHashEntry_Pr*));
    if (apNew == 0)
    {
        /* Not so fatal,simply a performance hit */
        return SXRET_OK;
    }
    /* Zero the new table */
    SyZero((void*)apNew, nNewSize * sizeof(SyHashEntry_Pr*));
    /* Rehash all entries */
    for (n = 0, pEntry = pHash->pList; n < pHash->nEntry; n++)
    {
        pEntry->pNextCollide = pEntry->pPrevCollide = 0;
        /* Install in the new bucket */
        iBucket = pEntry->nHash & (nNewSize - 1);
        pEntry->pNextCollide = apNew[iBucket];
        if (apNew[iBucket] != 0)
        {
            apNew[iBucket]->pPrevCollide = pEntry;
        }
        apNew[iBucket] = pEntry;
        /* Point to the next entry */
        pEntry = pEntry->pNext;
    }
    /* Release the old table and reflect the change */
    SyMemBackendFree(pHash->pAllocator, (void*)pHash->apBucket);
    pHash->apBucket = apNew;
    pHash->nBucketSize = nNewSize;

    return SXRET_OK;
}

static sxi32 HashInsert(SyHash* pHash, SyHashEntry_Pr* pEntry)
{
    sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);
    /* Insert the entry in its corresponding bcuket */
    pEntry->pNextCollide = pHash->apBucket[iBucket];
    if (pHash->apBucket[iBucket] != 0)
    {
        pHash->apBucket[iBucket]->pPrevCollide = pEntry;
    }
    pHash->apBucket[iBucket] = pEntry;
    /* Link to the entry list */
    MACRO_LD_PUSH(pHash->pList, pEntry);
    if (pHash->nEntry == 0)
    {
        pHash->pCurrent = pHash->pList;
    }
    pHash->nEntry++;

    return SXRET_OK;
}

PH7_PRIVATE sxi32 SyHashInsert(
    SyHash* pHash,
    const void* pKey,
    sxu32 nKeyLen,
    void* pUserData)
{
    sxi32 rc;

#if defined(UNTRUST)
    if (INVALID_HASH(pHash) || pKey == 0)
    {
        return SXERR_CORRUPT;
    }
#endif

    if (pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR)
    {
        rc = HashGrowTable(&(*pHash));
        if (rc != SXRET_OK)
        {
            return rc;
        }
    }
    /* Allocate a new hash entry */
    SyHashEntry_Pr* pEntry = (SyHashEntry_Pr*)SyMemBackendPoolAlloc(pHash->pAllocator, sizeof(SyHashEntry_Pr));
    if (pEntry == 0)
    {
        return SXERR_MEM;
    }
    /* Zero the entry */
    SyZero(pEntry, sizeof(SyHashEntry_Pr));
    pEntry->pHash = pHash;
    pEntry->pKey = pKey;
    pEntry->nKeyLen = nKeyLen;
    pEntry->pUserData = pUserData;
    pEntry->nHash = pHash->xHash(pEntry->pKey, pEntry->nKeyLen);
    /* Finally insert the entry in its corresponding bucket */
    rc = HashInsert(&(*pHash), pEntry);

    return rc;
}

PH7_PRIVATE SyHashEntry* SyHashLastEntry(SyHash* pHash)
{
#if defined(UNTRUST)
    if (INVALID_HASH(pHash))
    {
        return 0;
    }
#endif

    // Last inserted entry
    return (SyHashEntry*)pHash->pList;
}

/* SyRunTimeApi:sxutils.c */
PH7_PRIVATE sxi32 SyStrIsNumeric(
    const char* zSrc,
    sxu32 nLen,
    sxu8* pReal,
    const char** pzTail)
{
#ifdef UNTRUST
    if (SX_EMPTY_STR(zSrc))
    {
        return SXERR_EMPTY;
    }
#endif

    const char* zEnd = &zSrc[nLen];
    /* Jump leading white spaces */
    while (zSrc < zEnd && (unsigned char)zSrc[0] < 0xc0 && SyisSpace(zSrc[0]))
    {
        zSrc++;
    }
    if (zSrc < zEnd && (zSrc[0] == '+' || zSrc[0] == '-'))
    {
        zSrc++;
    }
    const char* zCur = zSrc;
    if (pReal)
    {
        *pReal = FALSE;
    }
    for (;;)
    {
        if (zSrc >= zEnd || (unsigned char)zSrc[0] >= 0xc0 || !SyisDigit(zSrc[0]))
        {
            break;
        }
        zSrc++;
        if (zSrc >= zEnd || (unsigned char)zSrc[0] >= 0xc0 || !SyisDigit(zSrc[0]))
        {
            break;
        }
        zSrc++;
        if (zSrc >= zEnd || (unsigned char)zSrc[0] >= 0xc0 || !SyisDigit(zSrc[0]))
        {
            break;
        }
        zSrc++;
        if (zSrc >= zEnd || (unsigned char)zSrc[0] >= 0xc0 || !SyisDigit(zSrc[0]))
        {
            break;
        }
        zSrc++;
    };
    if (zSrc < zEnd && zSrc > zCur)
    {
        int c = zSrc[0];
        if (c == '.')
        {
            zSrc++;
            if (pReal)
            {
                *pReal = TRUE;
            }
            if (pzTail)
            {
                while (zSrc < zEnd && (unsigned char)zSrc[0] < 0xc0 && SyisDigit(zSrc[0]))
                {
                    zSrc++;
                }
                if (zSrc < zEnd && (zSrc[0] == 'e' || zSrc[0] == 'E'))
                {
                    zSrc++;
                    if (zSrc < zEnd && (zSrc[0] == '+' || zSrc[0] == '-'))
                    {
                        zSrc++;
                    }
                    while (zSrc < zEnd && (unsigned char)zSrc[0] < 0xc0 && SyisDigit(zSrc[0]))
                    {
                        zSrc++;
                    }
                }
            }
        }
        else if (c == 'e' || c == 'E')
        {
            zSrc++;
            if (pReal != NULL)
            {
                *pReal = TRUE;
            }
            if (pzTail != NULL)
            {
                if (zSrc < zEnd && (zSrc[0] == '+' || zSrc[0] == '-'))
                {
                    zSrc++;
                }
                while (zSrc < zEnd && (unsigned char)zSrc[0] < 0xc0 && SyisDigit(zSrc[0]))
                {
                    zSrc++;
                }
            }
        }
    }
    if (pzTail != NULL)
    {
        /* Point to the non numeric part */
        *pzTail = zSrc;
    }

    return zSrc > zCur ?
           SXRET_OK /* String prefix is numeric */ :
           SXERR_INVALID /* Not a digit stream */;
}

#define SXINT32_MIN_STR        "2147483648"
#define SXINT32_MAX_STR        "2147483647"
#define SXINT64_MIN_STR        "9223372036854775808"
#define SXINT64_MAX_STR        "9223372036854775807"

PH7_PRIVATE sxi32 SyStrToInt32(
    const char* zSrc,
    sxu32 nLen,
    void* pOutVal,
    const char** zRest)
{
    int isNeg = FALSE;
    sxi32 nVal = 0;
    sxi16 i;

#if defined(UNTRUST)
    if (SX_EMPTY_STR(zSrc))
    {
        if (pOutVal)
        {
            *(sxi32*)
                pOutVal = 0;
        }
        return SXERR_EMPTY;
    }
#endif

    const char* zEnd = &zSrc[nLen];
    while (zSrc < zEnd && SyisSpace(zSrc[0]))
    {
        zSrc++;
    }
    if (zSrc < zEnd && (zSrc[0] == '-' || zSrc[0] == '+'))
    {
        isNeg = (zSrc[0] == '-') ? TRUE : FALSE;
        zSrc++;
    }
    /* Skip leading zero */
    while (zSrc < zEnd && zSrc[0] == '0')
    {
        zSrc++;
    }
    i = 10;
    if ((sxu32)(zEnd - zSrc) >= 10)
    {
        /* Handle overflow */
        i = SyMemcmp(zSrc, (isNeg == TRUE) ? SXINT32_MIN_STR : SXINT32_MAX_STR, nLen) <= 0 ? 10 : 9;
    }
    for (;;)
    {
        if (zSrc >= zEnd || !i || !SyisDigit(zSrc[0]))
        {
            break;
        }
        nVal = nVal * 10 + (zSrc[0] - '0');
        --i;
        zSrc++;
        if (zSrc >= zEnd || !i || !SyisDigit(zSrc[0]))
        {
            break;
        }
        nVal = nVal * 10 + (zSrc[0] - '0');
        --i;
        zSrc++;
        if (zSrc >= zEnd || !i || !SyisDigit(zSrc[0]))
        {
            break;
        }
        nVal = nVal * 10 + (zSrc[0] - '0');
        --i;
        zSrc++;
        if (zSrc >= zEnd || !i || !SyisDigit(zSrc[0]))
        {
            break;
        }
        nVal = nVal * 10 + (zSrc[0] - '0');
        --i;
        zSrc++;
    }
    /* Skip trailing spaces */
    while (zSrc < zEnd && SyisSpace(zSrc[0]))
    {
        zSrc++;
    }
    if (zRest)
    {
        *zRest = (char*)zSrc;
    }
    if (pOutVal)
    {
        if (isNeg == TRUE && nVal != 0)
        {
            nVal = -nVal;
        }
        *(sxi32*)
            pOutVal = nVal;
    }

    return (zSrc >= zEnd) ? SXRET_OK : SXERR_SYNTAX;
}

PH7_PRIVATE sxi32 SyStrToInt64(
    const char* zSrc,
    sxu32 nLen,
    void* pOutVal,
    const char** zRest)
{
    int isNeg = FALSE;
    const char* zEnd;
    sxi64 nVal;
    sxi16 i;

#if defined(UNTRUST)
    if (SX_EMPTY_STR(zSrc))
    {
        if (pOutVal)
        {
            *(sxi32*)
                pOutVal = 0;
        }
        return SXERR_EMPTY;
    }
#endif

    zEnd = &zSrc[nLen];
    while (zSrc < zEnd && SyisSpace(zSrc[0]))
    {
        zSrc++;
    }
    if (zSrc < zEnd && (zSrc[0] == '-' || zSrc[0] == '+'))
    {
        isNeg = (zSrc[0] == '-') ? TRUE : FALSE;
        zSrc++;
    }
    /* Skip leading zero */
    while (zSrc < zEnd && zSrc[0] == '0')
    {
        zSrc++;
    }
    i = 19;
    if ((sxu32)(zEnd - zSrc) >= 19)
    {
        i = SyMemcmp(zSrc, isNeg ? SXINT64_MIN_STR : SXINT64_MAX_STR, 19) <= 0 ? 19 : 18;
    }
    nVal = 0;
    for (;;)
    {
        if (zSrc >= zEnd || !i || !SyisDigit(zSrc[0]))
        {
            break;
        }
        nVal = nVal * 10 + (zSrc[0] - '0');
        --i;
        zSrc++;
        if (zSrc >= zEnd || !i || !SyisDigit(zSrc[0]))
        {
            break;
        }
        nVal = nVal * 10 + (zSrc[0] - '0');
        --i;
        zSrc++;
        if (zSrc >= zEnd || !i || !SyisDigit(zSrc[0]))
        {
            break;
        }
        nVal = nVal * 10 + (zSrc[0] - '0');
        --i;
        zSrc++;
        if (zSrc >= zEnd || !i || !SyisDigit(zSrc[0]))
        {
            break;
        }
        nVal = nVal * 10 + (zSrc[0] - '0');
        --i;
        zSrc++;
    }
    /* Skip trailing spaces */
    while (zSrc < zEnd && SyisSpace(zSrc[0]))
    {
        zSrc++;
    }
    if (zRest)
    {
        *zRest = (char*)zSrc;
    }
    if (pOutVal != NULL)
    {
        if (isNeg == TRUE && nVal != 0)
        {
            nVal = -nVal;
        }
        *(sxi64*)pOutVal = nVal;
    }

    return (zSrc >= zEnd) ? SXRET_OK : SXERR_SYNTAX;
}

PH7_PRIVATE sxi32 SyHexToint(sxi32 c)
{
    switch (c)
    {
        case '0':
            return 0;
        case '1':
            return 1;
        case '2':
            return 2;
        case '3':
            return 3;
        case '4':
            return 4;
        case '5':
            return 5;
        case '6':
            return 6;
        case '7':
            return 7;
        case '8':
            return 8;
        case '9':
            return 9;
        case 'A':
        case 'a':
            return 10;
        case 'B':
        case 'b':
            return 11;
        case 'C':
        case 'c':
            return 12;
        case 'D':
        case 'd':
            return 13;
        case 'E':
        case 'e':
            return 14;
        case 'F':
        case 'f':
            return 15;
        default:
            return -1;
    }
}

PH7_PRIVATE sxi32 SyHexStrToInt64(
    const char* zSrc,
    sxu32 nLen,
    void* pOutVal,
    const char** zRest)
{
    int isNeg = FALSE;
    sxi64 nVal = 0;

#if defined(UNTRUST)
    if (SX_EMPTY_STR(zSrc))
    {
        if (pOutVal)
        {
            *(sxi32*)
                pOutVal = 0;
        }
        return SXERR_EMPTY;
    }
#endif

    const char* zEnd = &zSrc[nLen];
    while (zSrc < zEnd && SyisSpace(zSrc[0]))
    {
        zSrc++;
    }
    if (zSrc < zEnd && (*zSrc == '-' || *zSrc == '+'))
    {
        isNeg = (zSrc[0] == '-') ? TRUE : FALSE;
        zSrc++;
    }
    if (zSrc < &zEnd[-2] && zSrc[0] == '0' && (zSrc[1] == 'x' || zSrc[1] == 'X'))
    {
        /* Bypass hex prefix */
        zSrc += sizeof(char) * 2;
    }
    /* Skip leading zero */
    while (zSrc < zEnd && zSrc[0] == '0')
    {
        zSrc++;
    }
    const char* zIn = zSrc;
    for (;;)
    {
        if (zSrc >= zEnd || !SyisHex(zSrc[0]) || (int)(zSrc - zIn) > 15)
        { break; }
        nVal = nVal * 16 + SyHexToint(zSrc[0]);
        zSrc++;
        if (zSrc >= zEnd || !SyisHex(zSrc[0]) || (int)(zSrc - zIn) > 15)
        { break; }
        nVal = nVal * 16 + SyHexToint(zSrc[0]);
        zSrc++;
        if (zSrc >= zEnd || !SyisHex(zSrc[0]) || (int)(zSrc - zIn) > 15)
        { break; }
        nVal = nVal * 16 + SyHexToint(zSrc[0]);
        zSrc++;
        if (zSrc >= zEnd || !SyisHex(zSrc[0]) || (int)(zSrc - zIn) > 15)
        { break; }
        nVal = nVal * 16 + SyHexToint(zSrc[0]);
        zSrc++;
    }
    while (zSrc < zEnd && SyisSpace(zSrc[0]))
    {
        zSrc++;
    }
    if (zRest)
    {
        *zRest = zSrc;
    }
    if (pOutVal != NULL)
    {
        if (isNeg == TRUE && nVal != 0)
        {
            nVal = -nVal;
        }
        *(sxi64*)pOutVal = nVal;
    }

    return zSrc >= zEnd ? SXRET_OK : SXERR_SYNTAX;
}

PH7_PRIVATE sxi32 SyOctalStrToInt64(
    const char* zSrc,
    sxu32 nLen,
    void* pOutVal,
    const char** zRest)
{
    int isNeg = FALSE;
    sxi64 nVal = 0;
    int c;

#if defined(UNTRUST)
    if (SX_EMPTY_STR(zSrc))
    {
        if (pOutVal)
        {
            *(sxi32*)pOutVal = 0;
        }
        return SXERR_EMPTY;
    }
#endif

    const char* zEnd = &zSrc[nLen];
    while (zSrc < zEnd && SyisSpace(zSrc[0]))
    {
        zSrc++;
    }
    if (zSrc < zEnd && (zSrc[0] == '-' || zSrc[0] == '+'))
    {
        isNeg = (zSrc[0] == '-') ? TRUE : FALSE;
        zSrc++;
    }
    /* Skip leading zero */
    while (zSrc < zEnd && zSrc[0] == '0')
    {
        zSrc++;
    }
    const char* zIn = zSrc;
    for (;;)
    {
        if (zSrc >= zEnd || !SyisDigit(zSrc[0]))
        {
            break;
        }
        if ((c = zSrc[0] - '0') > 7 || (int)(zSrc - zIn) > 20)
        {
            break;
        }
        nVal = nVal * 8 + c;
        zSrc++;
        if (zSrc >= zEnd || !SyisDigit(zSrc[0]))
        {
            break;
        }
        if ((c = zSrc[0] - '0') > 7 || (int)(zSrc - zIn) > 20)
        {
            break;
        }
        nVal = nVal * 8 + c;
        zSrc++;
        if (zSrc >= zEnd || !SyisDigit(zSrc[0]))
        {
            break;
        }
        if ((c = zSrc[0] - '0') > 7 || (int)(zSrc - zIn) > 20)
        {
            break;
        }
        nVal = nVal * 8 + c;
        zSrc++;
        if (zSrc >= zEnd || !SyisDigit(zSrc[0]))
        {
            break;
        }
        if ((c = zSrc[0] - '0') > 7 || (int)(zSrc - zIn) > 20)
        {
            break;
        }
        nVal = nVal * 8 + c;
        zSrc++;
    }
    /* Skip trailing spaces */
    while (zSrc < zEnd && SyisSpace(zSrc[0]))
    {
        zSrc++;
    }
    if (zRest)
    {
        *zRest = zSrc;
    }
    if (pOutVal)
    {
        if (isNeg == TRUE && nVal != 0)
        {
            nVal = -nVal;
        }
        *(sxi64*)pOutVal = nVal;
    }

    return (zSrc >= zEnd) ? SXRET_OK : SXERR_SYNTAX;
}

PH7_PRIVATE sxi32 SyBinaryStrToInt64(
    const char* zSrc,
    sxu32 nLen,
    void* pOutVal,
    const char** zRest)
{
    const char* zIn, * zEnd;
    int isNeg = FALSE;
    sxi64 nVal = 0;
    int c;

#if defined(UNTRUST)
    if (SX_EMPTY_STR(zSrc))
    {
        if (pOutVal)
        {
            *(sxi32*)pOutVal = 0;
        }
        return SXERR_EMPTY;
    }
#endif

    zEnd = &zSrc[nLen];
    while (zSrc < zEnd && SyisSpace(zSrc[0]))
    {
        zSrc++;
    }
    if (zSrc < zEnd && (zSrc[0] == '-' || zSrc[0] == '+'))
    {
        isNeg = (zSrc[0] == '-') ? TRUE : FALSE;
        zSrc++;
    }
    if (zSrc < &zEnd[-2] && zSrc[0] == '0' && (zSrc[1] == 'b' || zSrc[1] == 'B'))
    {
        /* Bypass binary prefix */
        zSrc += sizeof(char) * 2;
    }
    /* Skip leading zero */
    while (zSrc < zEnd && zSrc[0] == '0')
    {
        zSrc++;
    }
    zIn = zSrc;
    for (;;)
    {
        if (zSrc >= zEnd || (zSrc[0] != '1' && zSrc[0] != '0') || (int)(zSrc - zIn) > 62)
        {
            break;
        }
        c = zSrc[0] - '0';
        nVal = (nVal << 1u) + c;
        zSrc++;
        if (zSrc >= zEnd || (zSrc[0] != '1' && zSrc[0] != '0') || (int)(zSrc - zIn) > 62)
        {
            break;
        }
        c = zSrc[0] - '0';
        nVal = (nVal << 1u) + c;
        zSrc++;
        if (zSrc >= zEnd || (zSrc[0] != '1' && zSrc[0] != '0') || (int)(zSrc - zIn) > 62)
        {
            break;
        }
        c = zSrc[0] - '0';
        nVal = (nVal << 1u) + c;
        zSrc++;
        if (zSrc >= zEnd || (zSrc[0] != '1' && zSrc[0] != '0') || (int)(zSrc - zIn) > 62)
        {
            break;
        }
        c = zSrc[0] - '0';
        nVal = (nVal << 1u) + c;
        zSrc++;
    }
    /* Skip trailing spaces */
    while (zSrc < zEnd && SyisSpace(zSrc[0]))
    {
        zSrc++;
    }
    if (zRest)
    {
        *zRest = zSrc;
    }
    if (pOutVal != NULL)
    {
        if (isNeg == TRUE && nVal != 0)
        {
            nVal = -nVal;
        }
        *(sxi64*)pOutVal = nVal;
    }

    return (zSrc >= zEnd) ? SXRET_OK : SXERR_SYNTAX;
}

PH7_PRIVATE sxi32 SyStrToReal(
    const char* zSrc,
    sxu32 nLen,
    void* pOutVal,
    const char** zRest)
{
#define SXDBL_DIG           15
#define SXDBL_MAX_EXP       308
#define SXDBL_MIN_EXP_PLUS  307

    static const sxreal aTab[] = {
        10,
        1.0e2,
        1.0e4,
        1.0e8,
        1.0e16,
        1.0e32,
        1.0e64,
        1.0e128,
        1.0e256
    };
    sxu8 neg = FALSE;
    sxreal Val = 0.0;
    const char* zEnd;
    sxi32 Lim, exp;
    sxreal* p = 0;

#ifdef UNTRUST
    if (SX_EMPTY_STR(zSrc))
    {
        if (pOutVal)
        {
            *(sxreal*)pOutVal = 0;
        }
        return SXERR_EMPTY;
    }
#endif

    zEnd = &zSrc[nLen];
    while (zSrc < zEnd && SyisSpace(zSrc[0]))
    {
        zSrc++;
    }
    if (zSrc < zEnd && (zSrc[0] == '-' || zSrc[0] == '+'))
    {
        neg = zSrc[0] == '-' ? TRUE : FALSE;
        zSrc++;
    }
    Lim = SXDBL_DIG;
    for (;;)
    {
        if (zSrc >= zEnd || Lim == 0 || !SyisDigit(zSrc[0]))
        {
            break;
        }
        Val = Val * 10 + (zSrc[0] - '0');
        zSrc++;
        Lim--;
        if (zSrc >= zEnd || Lim == 0 || !SyisDigit(zSrc[0]))
        {
            break;
        }
        Val = Val * 10 + (zSrc[0] - '0');
        zSrc++;
        Lim--;
        if (zSrc >= zEnd || Lim == 0 || !SyisDigit(zSrc[0]))
        {
            break;
        }
        Val = Val * 10 + (zSrc[0] - '0');
        zSrc++;
        Lim--;
        if (zSrc >= zEnd || Lim == 0 || !SyisDigit(zSrc[0]))
        {
            break;
        }
        Val = Val * 10 + (zSrc[0] - '0');
        zSrc++;
        Lim--;
    }
    if (zSrc < zEnd && (zSrc[0] == '.' || zSrc[0] == ','))
    {
        sxreal dec = 1;
        zSrc++;
        for (;;)
        {
            if (zSrc >= zEnd || !Lim || !SyisDigit(zSrc[0]))
            {
                break;
            }
            Val = Val * 10 + (zSrc[0] - '0');
            dec *= 10;
            zSrc++;
            Lim--;
            if (zSrc >= zEnd || !Lim || !SyisDigit(zSrc[0]))
            {
                break;
            }
            Val = Val * 10 + (zSrc[0] - '0');
            dec *= 10;
            zSrc++;
            Lim--;
            if (zSrc >= zEnd || !Lim || !SyisDigit(zSrc[0]))
            {
                break;
            }
            Val = Val * 10 + (zSrc[0] - '0');
            dec *= 10;
            zSrc++;
            Lim--;
            if (zSrc >= zEnd || !Lim || !SyisDigit(zSrc[0]))
            {
                break;
            }
            Val = Val * 10 + (zSrc[0] - '0');
            dec *= 10;
            zSrc++;
            Lim--;
        }
        Val /= dec;
    }
    if (neg == TRUE && Val != 0)
    {
        Val = -Val;
    }
    if (Lim <= 0)
    {
        /* jump overflow digit */
        while (zSrc < zEnd)
        {
            if (zSrc[0] == 'e' || zSrc[0] == 'E')
            {
                break;
            }
            zSrc++;
        }
    }
    neg = FALSE;
    if (zSrc < zEnd && (zSrc[0] == 'e' || zSrc[0] == 'E'))
    {
        zSrc++;
        if (zSrc < zEnd && (zSrc[0] == '-' || zSrc[0] == '+'))
        {
            neg = zSrc[0] == '-' ? TRUE : FALSE;
            zSrc++;
        }
        exp = 0;
        while (zSrc < zEnd && SyisDigit(zSrc[0]) && exp < SXDBL_MAX_EXP)
        {
            exp = exp * 10 + (zSrc[0] - '0');
            zSrc++;
        }
        if (neg)
        {
            if (exp > SXDBL_MIN_EXP_PLUS)
            { exp = SXDBL_MIN_EXP_PLUS; }
        }
        else if (exp > SXDBL_MAX_EXP)
        {
            exp = SXDBL_MAX_EXP;
        }
        for (p = (sxreal*)aTab; exp; exp >>= 1u, p++)
        {
            if (exp & 01u)
            {
                if (neg)
                {
                    Val /= *p;
                }
                else
                {
                    Val *= *p;
                }
            }
        }
    }
    while (zSrc < zEnd && SyisSpace(zSrc[0]))
    {
        zSrc++;
    }
    if (zRest)
    {
        *zRest = zSrc;
    }
    if (pOutVal != NULL)
    {
        *(sxreal*)pOutVal = Val;
    }

    return zSrc >= zEnd ? SXRET_OK : SXERR_SYNTAX;
}

/* SyRunTimeApi:sxlib.c  */
static sxu32 SyBinHash(const void* pSrc, sxu32 nLen)
{
    register unsigned char* zIn = (unsigned char*)pSrc;
    sxu32 nH = 5381;
    unsigned char* zEnd = &zIn[nLen];

    for (;;)
    {
        if (zIn >= zEnd)
        {
            break;
        }
        nH = nH * 33 + zIn[0];
        zIn++;
        if (zIn >= zEnd)
        {
            break;
        }
        nH = nH * 33 + zIn[0];
        zIn++;
        if (zIn >= zEnd)
        {
            break;
        }
        nH = nH * 33 + zIn[0];
        zIn++;
        if (zIn >= zEnd)
        {
            break;
        }
        nH = nH * 33 + zIn[0];
        zIn++;
    }

    return nH;
}

PH7_PRIVATE sxu32 SyStrHash(const void* pSrc, sxu32 nLen)
{
    register unsigned char* zIn = (unsigned char*)pSrc;
    sxu32 nH = 5381;
    unsigned char* zEnd = &zIn[nLen];

    for (;;)
    {
        if (zIn >= zEnd)
        {
            break;
        }
        nH = nH * 33 + SyToLower(zIn[0]);
        zIn++;
        if (zIn >= zEnd)
        {
            break;
        }
        nH = nH * 33 + SyToLower(zIn[0]);
        zIn++;
        if (zIn >= zEnd)
        {
            break;
        }
        nH = nH * 33 + SyToLower(zIn[0]);
        zIn++;
        if (zIn >= zEnd)
        {
            break;
        }
        nH = nH * 33 + SyToLower(zIn[0]);
        zIn++;
    }

    return nH;
}

#ifndef PH7_DISABLE_BUILTIN_FUNC

PH7_PRIVATE sxi32 SyBase64Encode(
    const char* zSrc,
    sxu32 nLen,
    ProcConsumer xConsumer,
    void* pUserData)
{
    static const unsigned char zBase64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    unsigned char* zIn = (unsigned char*)zSrc;
    unsigned char z64[4];
    sxi32 rc;

#if defined(UNTRUST)
    if (SX_EMPTY_STR(zSrc) || xConsumer == 0)
    {
        return SXERR_EMPTY;
    }
#endif
    sxu32 i;
    for (i = 0; i + 2 < nLen; i += 3)
    {
        z64[0] = zBase64[(zIn[i] >> 2) & 0x3F];
        z64[1] = zBase64[(((zIn[i] & 0x03) << 4) | (zIn[i + 1] >> 4)) & 0x3F];
        z64[2] = zBase64[(((zIn[i + 1] & 0x0F) << 2) | (zIn[i + 2] >> 6)) & 0x3F];
        z64[3] = zBase64[zIn[i + 2] & 0x3F];

        rc = xConsumer((const void*)z64, sizeof(z64), pUserData);
        if (rc != SXRET_OK)
        {
            return SXERR_ABORT;
        }
    }
    if (i + 1 < nLen)
    {
        z64[0] = zBase64[(zIn[i] >> 2u) & 0x3Fu];
        z64[1] = zBase64[(((zIn[i] & 0x03) << 4) | (zIn[i + 1] >> 4)) & 0x3F];
        z64[2] = zBase64[(zIn[i + 1] & 0x0Fu) << 2u];
        z64[3] = '=';

        rc = xConsumer((const void*)z64, sizeof(z64), pUserData);
        if (rc != SXRET_OK)
        {
            return SXERR_ABORT;
        }
    }
    else if (i < nLen)
    {
        z64[0] = zBase64[(zIn[i] >> 2u) & 0x3Fu];
        z64[1] = zBase64[(zIn[i] & 0x03u) << 4u];
        z64[2] = '=';
        z64[3] = '=';

        rc = xConsumer((const void*)z64, sizeof(z64), pUserData);
        if (rc != SXRET_OK)
        {
            return SXERR_ABORT;
        }
    }

    return SXRET_OK;
}

PH7_PRIVATE sxi32 SyBase64Decode(
    const char* zB64,
    sxu32 nLen,
    ProcConsumer xConsumer,
    void* pUserData)
{
    static const sxu32 aBase64Trans[] = {
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0,
        0,
        0, 0, 0, 0, 0, 62, 0, 0, 0, 63, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 0, 0, 0, 0, 0, 0, 0, 0, 1, 2, 3, 4,
        5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 0, 0, 0, 0, 0, 0, 26, 27,
        28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 0, 0,
        0, 0, 0
    };
    sxu32 n, w, x, y, z;
    sxi32 rc;
    unsigned char zOut[10];

#if defined(UNTRUST)
    if (SX_EMPTY_STR(zB64) || xConsumer == 0)
    {
        return SXERR_EMPTY;
    }
#endif

    while (nLen > 0 && zB64[nLen - 1] == '=')
    {
        nLen--;
    }
    for (n = 0; n + 3 < nLen; n += 4)
    {
        w = aBase64Trans[zB64[n] & 0x7F];
        x = aBase64Trans[zB64[n + 1] & 0x7F];
        y = aBase64Trans[zB64[n + 2] & 0x7F];
        z = aBase64Trans[zB64[n + 3] & 0x7F];
        zOut[0] = ((w << 2) & 0xFC) | ((x >> 4) & 0x03);
        zOut[1] = ((x << 4) & 0xF0) | ((y >> 2) & 0x0F);
        zOut[2] = ((y << 6) & 0xC0) | (z & 0x3F);

        rc = xConsumer((const void*)zOut, sizeof(unsigned char) * 3, pUserData);
        if (rc != SXRET_OK)
        {
            return SXERR_ABORT;
        }
    }
    if (n + 2 < nLen)
    {
        w = aBase64Trans[zB64[n] & 0x7F];
        x = aBase64Trans[zB64[n + 1] & 0x7F];
        y = aBase64Trans[zB64[n + 2] & 0x7F];

        zOut[0] = ((w << 2) & 0xFC) | ((x >> 4) & 0x03);
        zOut[1] = ((x << 4) & 0xF0) | ((y >> 2) & 0x0F);

        rc = xConsumer((const void*)zOut, sizeof(unsigned char) * 2, pUserData);
        if (rc != SXRET_OK)
        {
            return SXERR_ABORT;
        }
    }
    else if (n + 1 < nLen)
    {
        w = aBase64Trans[zB64[n] & 0x7F];
        x = aBase64Trans[zB64[n + 1] & 0x7F];

        zOut[0] = ((w << 2u) & 0xFCu) | ((x >> 4u) & 0x03u);

        rc = xConsumer((const void*)zOut, sizeof(unsigned char) * 1, pUserData);
        if (rc != SXRET_OK)
        {
            return SXERR_ABORT;
        }
    }

    return SXRET_OK;
}

#endif // PH7_DISABLE_BUILTIN_FUNC

#define INVALID_LEXER(LEX) (LEX == 0  || LEX->xTokenizer == 0)

PH7_PRIVATE sxi32 SyLexInit(
    SyLex* pLex,
    SySet* pSet,
    ProcTokenizer xTokenizer,
    void* pUserData)
{
#if defined (UNTRUST)
    if (pLex == 0 || xTokenizer == 0)
    {
        return SXERR_CORRUPT;
    }
#endif

    pLex->pTokenSet = 0;
    /* Initialize lexer fields */
    if (pSet != NULL)
    {
        if (SySetElemSize(pSet) != sizeof(SyToken))
        {
            return SXERR_INVALID;
        }
        pLex->pTokenSet = pSet;
    }
    SyStream* pStream = &pLex->sStream;
    pLex->xTokenizer = xTokenizer;
    pLex->pUserData = pUserData;

    pStream->nLine = 1;
    pStream->nIgn = 0;
    pStream->zText = pStream->zEnd = 0;
    pStream->pSet = pSet;

    return SXRET_OK;
}

PH7_PRIVATE sxi32 SyLexTokenizeInput(
    SyLex* pLex,
    const char* zInput,
    sxu32 nLen,
    void* pCtxData,
    ProcSort xSort,
    ProcCmp xCmp)
{
#if defined (UNTRUST)
    if (INVALID_LEXER(pLex) || zInput == 0)
    {
        return SXERR_CORRUPT;
    }
#endif

    SyStream* pStream = &pLex->sStream;
    /* Point to the head of the input */
    pStream->zText = pStream->zInput = (const unsigned char*)zInput;
    /* Point to the end of the input */
    pStream->zEnd = &pStream->zInput[nLen];
    for (;;)
    {
        if (pStream->zText >= pStream->zEnd)
        {
            /* End of the input reached */
            break;
        }
        const unsigned char* zCur = pStream->zText;
        SyToken sToken;
        /* Call the tokenizer callback */
        sxi32 rc = pLex->xTokenizer(pStream, &sToken, pLex->pUserData, pCtxData);
        if (rc != SXRET_OK && rc != SXERR_CONTINUE)
        {
            /* Tokenizer callback request an operation abort */
            if (rc == SXERR_ABORT)
            {
                return SXERR_ABORT;
            }
            break;
        }
        if (rc == SXERR_CONTINUE)
        {
            /* Request to ignore this token */
            pStream->nIgn++;
        }
        else if (pLex->pTokenSet)
        {
            /* Put the token in the set */
            rc = SySetPut(pLex->pTokenSet, (const void*)&sToken);
            if (rc != SXRET_OK)
            {
                break;
            }
        }
        if (zCur >= pStream->zText)
        {
            /* Automatic advance of the stream cursor */
            pStream->zText = &zCur[1];
        }
    }
    if (xSort && pLex->pTokenSet)
    {
        SyToken* aToken = (SyToken*)SySetBasePtr(pLex->pTokenSet);
        /* Sort the extrated tokens */
        if (xCmp == 0)
        {
            /* Use a default comparison function */
            xCmp = SyMemcmp;
        }
        xSort(aToken, SySetUsed(pLex->pTokenSet), sizeof(SyToken), xCmp);
    }

    return SXRET_OK;
}

PH7_PRIVATE sxi32 SyLexRelease(SyLex* pLex)
{
    sxi32 rc = SXRET_OK;

#if defined (UNTRUST)
    if (INVALID_LEXER(pLex))
    {
        return SXERR_CORRUPT;
    }
#else
    // Prevent compiler warning
    SXUNUSED(pLex);
#endif

    return rc;
}

#ifndef PH7_DISABLE_BUILTIN_FUNC
#define SAFE_HTTP(C) (SyisAlphaNum(c) || c == '_' || c == '-' || c == '$' || c == '.' )

PH7_PRIVATE sxi32 SyUriEncode(
    const char* zSrc,
    sxu32 nLen,
    ProcConsumer xConsumer,
    void* pUserData)
{
    unsigned char* zIn = (unsigned char*)zSrc;
    unsigned char zHex[3] = {'%', 0, 0};
    unsigned char zOut[2];
    unsigned char* zCur, * zEnd;

#ifdef UNTRUST
    if (SX_EMPTY_STR(zSrc) || xConsumer == 0)
    {
        return SXERR_EMPTY;
    }
#endif

    sxi32 rc = SXRET_OK;
    zEnd = &zIn[nLen];
    zCur = zIn;
    for (;;)
    {
        if (zCur >= zEnd)
        {
            if (zCur != zIn)
            {
                rc = xConsumer(zIn, (sxu32)(zCur - zIn), pUserData);
            }
            break;
        }
        sxi32 c = zCur[0];
        if (SAFE_HTTP(c))
        {
            zCur++;
            continue;
        }
        if (zCur != zIn && SXRET_OK != (rc = xConsumer(zIn, (sxu32)(zCur - zIn), pUserData)))
        {
            break;
        }
        if (c == ' ')
        {
            zOut[0] = '+';
            rc = xConsumer((const void*)zOut, sizeof(unsigned char), pUserData);
        }
        else
        {
            zHex[1] = "0123456789ABCDEF"[(c >> 4u) & 0x0F];
            zHex[2] = "0123456789ABCDEF"[(c) & 0x0F];
            rc = xConsumer(zHex, sizeof(zHex), pUserData);
        }
        if (SXRET_OK != rc)
        {
            break;
        }
        zIn = &zCur[1];
        zCur = zIn;
    }

    return rc == SXRET_OK ? SXRET_OK : SXERR_ABORT;
}

#endif // PH7_DISABLE_BUILTIN_FUNC

static sxi32 SyAsciiToHex(sxi32 c)
{
    if (c >= 'a' && c <= 'f')
    {
        c += 10 - 'a';
        return c;
    }
    if (c >= '0' && c <= '9')
    {
        c -= '0';
        return c;
    }
    if (c >= 'A' && c <= 'F')
    {
        c += 10 - 'A';
        return c;
    }

    return 0;
}

PH7_PRIVATE sxi32 SyUriDecode(
    const char* zSrc,
    sxu32 nLen,
    ProcConsumer xConsumer,
    void* pUserData,
    int bUTF8)
{
    static const sxu8 Utf8Trans[] = {
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
        0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
        0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
        0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f,
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
        0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
        0x00, 0x01, 0x02, 0x03, 0x00, 0x01, 0x00, 0x00
    };
    const char* zIn = zSrc;
    const char* zEnd;
    const char* zCur;
    sxu8* zOutPtr;
    sxu8 zOut[10];
    sxi32 c, d;
    sxi32 rc;

#if defined(UNTRUST)
    if (SX_EMPTY_STR(zSrc) || xConsumer == 0)
    {
        return SXERR_EMPTY;
    }
#endif

    rc = SXRET_OK;
    zEnd = &zSrc[nLen];
    zCur = zIn;
    for (;;)
    {
        while (zCur < zEnd && zCur[0] != '%' && zCur[0] != '+')
        {
            zCur++;
        }
        if (zCur != zIn)
        {
            /* Consume input */
            rc = xConsumer(zIn, (unsigned int)(zCur - zIn), pUserData);
            if (rc != SXRET_OK)
            {
                /* User consumer routine request an operation abort */
                break;
            }
        }
        if (zCur >= zEnd)
        {
            rc = SXRET_OK;
            break;
        }
        /* Decode unsafe HTTP characters */
        zOutPtr = zOut;
        if (zCur[0] == '+')
        {
            *zOutPtr++ = ' ';
            zCur++;
        }
        else
        {
            if (&zCur[2] >= zEnd)
            {
                rc = SXERR_OVERFLOW;
                break;
            }
            c = (SyAsciiToHex(zCur[1]) << 4) | SyAsciiToHex(zCur[2]);
            zCur += 3;
            if (c < 0x000C0)
            {
                *zOutPtr++ = (sxu8)c;
            }
            else
            {
                c = Utf8Trans[c - 0xC0];
                while (zCur[0] == '%')
                {
                    d = (SyAsciiToHex(zCur[1]) << 4) | SyAsciiToHex(zCur[2]);
                    if ((d & 0xC0) != 0x80)
                    {
                        break;
                    }
                    c = (c << 6) + (0x3f & d);
                    zCur += 3;
                }
                if (bUTF8 == FALSE)
                {
                    *zOutPtr++ = (sxu8)c;
                }
                else
                {
                    SX_WRITE_UTF8(zOutPtr, c);
                }
            }

        }
        /* Consume the decoded characters */
        rc = xConsumer((const void*)zOut, (unsigned int)(zOutPtr - zOut), pUserData);
        if (rc != SXRET_OK)
        {
            break;
        }
        /* Synchronize pointers */
        zIn = zCur;
    }

    return rc;
}

#ifndef PH7_DISABLE_BUILTIN_FUNC
static const char* zEngDay[] = {
    "Sunday", "Monday", "Tuesday", "Wednesday",
    "Thursday", "Friday", "Saturday"
};
static const char* zEngMonth[] = {
    "January", "February", "March", "April",
    "May", "June", "July", "August",
    "September", "October", "November", "December"
};

static const char* GetDay(sxi32 i)
{
    return zEngDay[i % 7];
}

static const char* GetMonth(sxi32 i)
{
    return zEngMonth[i % 12];
}

PH7_PRIVATE const char* SyTimeGetDay(sxi32 iDay)
{
    return GetDay(iDay);
}

PH7_PRIVATE const char* SyTimeGetMonth(sxi32 iMonth)
{
    return GetMonth(iMonth);
}

#endif /* PH7_DISABLE_BUILTIN_FUNC */
/* SyRunTimeApi: sxfmt.c */
#define SXFMT_BUFSIZ 1024 /* Conversion buffer size */
/*
** Conversion types fall into various categories as defined by the
** following enumeration.
*/
#define SXFMT_RADIX       1 /* Integer types.%d, %x, %o, and so forth */
#define SXFMT_FLOAT       2 /* Floating point.%f */
#define SXFMT_EXP         3 /* Exponentional notation.%e and %E */
#define SXFMT_GENERIC     4 /* Floating or exponential, depending on exponent.%g */
#define SXFMT_SIZE        5 /* Total number of characters processed so far.%n */
#define SXFMT_STRING      6 /* Strings.%s */
#define SXFMT_PERCENT     7 /* Percent symbol.%% */
#define SXFMT_CHARX       8 /* Characters.%c */
#define SXFMT_ERROR       9 /* Used to indicate no such conversion type */
/* Extension by Symisc Systems */
#define SXFMT_RAWSTR     13 /* %z Pointer to raw string (SyString *) */
#define SXFMT_UNUSED     15
/*
** Allowed values for SyFmtInfo.flags
*/
#define SXFLAG_SIGNED    0x01
#define SXFLAG_UNSIGNED 0x02
/* Allowed values for SyFmtConsumer.nType */
#define SXFMT_CONS_PROC        1    /* Consumer is a procedure */
#define SXFMT_CONS_STR        2    /* Consumer is a managed string */
#define SXFMT_CONS_FILE        5    /* Consumer is an open File */
#define SXFMT_CONS_BLOB        6    /* Consumer is a BLOB */

/*
** Each builtin conversion character (ex: the 'd' in "%d") is described
** by an instance of the following structure
*/
typedef struct SyFmtInfo SyFmtInfo;
struct SyFmtInfo
{
    char fmttype;  /* The format field code letter [i.e: 'd','s','x'] */
    sxu8 base;     /* The base for radix conversion */
    int flags;    /* One or more of SXFLAG_ constants below */
    sxu8 type;     /* Conversion paradigm */
    char* charset; /* The character set for conversion */
    char* prefix;  /* Prefix on non-zero values in alt format */
};

typedef struct SyFmtConsumer SyFmtConsumer;
struct SyFmtConsumer
{
    sxu32 nLen; /* Total output length */
    sxi32 nType; /* Type of the consumer see below */
    sxi32 rc;    /* Consumer return value;Abort processing if rc != SXRET_OK */
    union
    {
        struct
        {
            ProcConsumer xUserConsumer;
            void* pUserData;
        } sFunc;
        SyBlob* pBlob;
    } uConsumer;
};

#ifndef SX_OMIT_FLOATINGPOINT

static int getdigit(sxlongreal* val, int* cnt)
{
    if ((*cnt)++ >= 16)
    {
        return '0';
    }
    int digit = (int)*val;
    sxlongreal d = digit;
    *val = (*val - d) * 10;

    return digit + '0';
}

#endif // SX_OMIT_FLOATINGPOINT

/*
 * The following routine was taken from the SQLITE2 source tree and was
 * extended by Symisc Systems to fit its need.
 * Status: Public Domain
 */
static sxi32 InternFormat(
    ProcConsumer xConsumer,
    void* pUserData,
    const char* zFormat,
    va_list ap)
{
    /*
     * The following table is searched linearly, so it is good to put the most frequently
     * used conversion types first.
     */
    static const SyFmtInfo aFmt[] = {
        {'d', 10, SXFLAG_SIGNED, SXFMT_RADIX, "0123456789", 0},
        {'s', 0, 0, SXFMT_STRING, 0, 0},
        {'c', 0, 0, SXFMT_CHARX, 0, 0},
        {'x', 16, 0, SXFMT_RADIX, "0123456789abcdef", "x0"},
        {'X', 16, 0, SXFMT_RADIX, "0123456789ABCDEF", "X0"},
        /* -- Extensions by Symisc Systems -- */
        {'z', 0, 0, SXFMT_RAWSTR, 0, 0}, /* Pointer to a raw string (SyString *) */
        {'B', 2, 0, SXFMT_RADIX, "01", "b0"},
        /* -- End of Extensions -- */
        {'o', 8, 0, SXFMT_RADIX, "01234567", "0"},
        {'u', 10, 0, SXFMT_RADIX, "0123456789", 0},
#ifndef SX_OMIT_FLOATINGPOINT
        {'f', 0, SXFLAG_SIGNED, SXFMT_FLOAT, 0, 0},
        {'e', 0, SXFLAG_SIGNED, SXFMT_EXP, "e", 0},
        {'E', 0, SXFLAG_SIGNED, SXFMT_EXP, "E", 0},
        {'g', 0, SXFLAG_SIGNED, SXFMT_GENERIC, "e", 0},
        {'G', 0, SXFLAG_SIGNED, SXFMT_GENERIC, "E", 0},
#endif
        {'i', 10, SXFLAG_SIGNED, SXFMT_RADIX, "0123456789", 0},
        {'n', 0, 0, SXFMT_SIZE, 0, 0},
        {'%', 0, 0, SXFMT_PERCENT, 0, 0},
        {'p', 10, 0, SXFMT_RADIX, "0123456789", 0}
    };

    int c;                     /* Next character in the format string */
    char* bufpt;               /* Pointer to the conversion buffer */
    int precision;             /* Precision of the current field */
    int length;                /* Length of the field */
    int idx;                   /* A general purpose loop counter */
    int width;                 /* Width of the current field */
    sxu8 flag_leftjustify;   /* True if "-" flag is present */
    sxu8 flag_plussign;      /* True if "+" flag is present */
    sxu8 flag_blanksign;     /* True if " " flag is present */
    sxu8 flag_alternateform; /* True if "#" flag is present */
    sxu8 flag_zeropad;       /* True if field width constant starts with zero */
    sxu8 flag_long;          /* True if "l" flag is present */
    sxi64 longvalue;         /* Value for integer types */
    const SyFmtInfo* infop;  /* Pointer to the appropriate info structure */
    char buf[SXFMT_BUFSIZ];  /* Conversion buffer */
    char prefix;             /* Prefix character."+" or "-" or " " or '\0'.*/
    sxu8 errorflag = 0;      /* True if an error is encountered */
    sxu8 xtype;              /* Conversion paradigm */
    char* zExtra;
    static char spaces[] = "                                                  ";
#define etSPACESIZE ((int)sizeof(spaces)-1)
#ifndef SX_OMIT_FLOATINGPOINT
    sxlongreal realvalue;    /* Value for real types */
    int exp;                /* exponent of real numbers */
    double rounder;          /* Used for rounding floating point values */
    sxu8 flag_dp;            /* True if decimal point should be shown */
    sxu8 flag_rtz;           /* True if trailing zeros should be removed */
    sxu8 flag_exp;           /* True to force display of the exponent */
    int nsd;                 /* Number of significant digits returned */
#endif
    int rc;

    length = 0;
    bufpt = 0;
    for (; (c = (*zFormat)) != 0; ++zFormat)
    {
        if (c != '%')
        {
            unsigned int amt;
            bufpt = (char*)zFormat;
            amt = 1;
            while ((c = (*++zFormat)) != '%' && c != 0) amt++;
            rc = xConsumer((const void*)bufpt, amt, pUserData);
            if (rc != SXRET_OK)
            {
                // Consumer routine request an operation abort
                return SXERR_ABORT;
            }
            if (c == 0)
            {
                return errorflag > 0 ? SXERR_FORMAT : SXRET_OK;
            }
        }
        if ((c = (*++zFormat)) == 0)
        {
            errorflag = 1;
            rc = xConsumer("%", sizeof("%") - 1, pUserData);
            if (rc != SXRET_OK)
            {
                // Consumer routine request an operation abort
                return SXERR_ABORT;
            }

            return errorflag > 0 ? SXERR_FORMAT : SXRET_OK;
        }
        /* Find out what flags are present */
        flag_leftjustify = flag_plussign = flag_blanksign =
        flag_alternateform = flag_zeropad = 0;
        do
        {
            switch (c)
            {
                case '-':
                    flag_leftjustify = 1;
                    c = 0;
                    break;
                case '+':
                    flag_plussign = 1;
                    c = 0;
                    break;
                case ' ':
                    flag_blanksign = 1;
                    c = 0;
                    break;
                case '#':
                    flag_alternateform = 1;
                    c = 0;
                    break;
                case '0':
                    flag_zeropad = 1;
                    c = 0;
                    break;
                default:
                    break;
            }
        } while (c == 0 && (c = (*++zFormat)) != 0);
        /* Get the field width */
        width = 0;
        if (c == '*')
        {
            width = va_arg(ap, int);
            if (width < 0)
            {
                flag_leftjustify = 1;
                width = -width;
            }
            c = *++zFormat;
        }
        else
        {
            while (c >= '0' && c <= '9')
            {
                width = width * 10 + c - '0';
                c = *++zFormat;
            }
        }
        if (width > SXFMT_BUFSIZ - 10)
        {
            width = SXFMT_BUFSIZ - 10;
        }
        /* Get the precision */
        precision = -1;
        if (c == '.')
        {
            precision = 0;
            c = *++zFormat;
            if (c == '*')
            {
                precision = va_arg(ap, int);
                if (precision < 0)
                { precision = -precision; }
                c = *++zFormat;
            }
            else
            {
                while (c >= '0' && c <= '9')
                {
                    precision = precision * 10 + c - '0';
                    c = *++zFormat;
                }
            }
        }
        /* Get the conversion type modifier */
        flag_long = 0;
        if (c == 'l' || c == 'q' /* BSD quad (expect a 64-bit integer) */ )
        {
            flag_long = (c == 'q') ? 2 : 1;
            c = *++zFormat;
            if (c == 'l')
            {
                /* Standard printf emulation 'lld' (expect a 64bit integer) */
                flag_long = 2;
            }
        }
        /* Fetch the info entry for the field */
        infop = 0;
        xtype = SXFMT_ERROR;
        for (idx = 0; idx < (int)SX_ARRAYSIZE(aFmt); idx++)
        {
            if (c == aFmt[idx].fmttype)
            {
                infop = &aFmt[idx];
                xtype = infop->type;
                break;
            }
        }
        zExtra = 0;

        /*
    ** At this point, variables are initialized as follows:
    **
    **   flag_alternateform          TRUE if a '#' is present.
    **   flag_plussign               TRUE if a '+' is present.
    **   flag_leftjustify            TRUE if a '-' is present or if the
    **                               field width was negative.
    **   flag_zeropad                TRUE if the width began with 0.
    **   flag_long                   TRUE if the letter 'l' (ell) or 'q'(BSD quad) prefixed
    **                               the conversion character.
    **   flag_blanksign              TRUE if a ' ' is present.
    **   width                       The specified field width.This is
    **                               always non-negative.Zero is the default.
    **   precision                   The specified precision.The default
    **                               is -1.
    **   xtype                       The class of the conversion.
    **   infop                       Pointer to the appropriate info struct.
    */
        switch (xtype)
        {
            case SXFMT_RADIX:
            {
                if (flag_long > 0)
                {
                    if (flag_long > 1)
                    {
                        /* BSD quad: expect a 64-bit integer */
                        longvalue = va_arg(ap, sxi64);
                    }
                    else
                    {
                        longvalue = va_arg(ap, sxlong);
                    }
                }
                else
                {
                    if (infop->flags & SXFLAG_SIGNED)
                    {
                        longvalue = va_arg(ap, sxi32);
                    }
                    else
                    {
                        longvalue = va_arg(ap, sxu32);
                    }
                }
                // Limit the precision to prevent overflowing buf[] during conversion.
                if (precision > SXFMT_BUFSIZ - 40)
                {
                    precision = SXFMT_BUFSIZ - 40;
                }
#if 0
                // For the format %#x, the value zero is printed "0" not "0x0". I think this is stupid.
                if (longvalue == 0)
                {
                    flag_alternateform = 0;
                }
#else
                // More sensible: turn off the prefix for octal (to prevent "00"), but leave the prefix for hex.
                if (longvalue == 0 && infop->base == 8)
                {
                    flag_alternateform = 0;
                }
#endif
                if (infop->flags & SXFLAG_SIGNED)
                {
                    if (longvalue < 0)
                    {
                        longvalue = -longvalue;
                        /* Ticket 1433-003 */
                        if (longvalue < 0)
                        {
                            /* Overflow */
                            longvalue = 0x7FFFFFFFFFFFFFFF;
                        }
                        prefix = '-';
                    }
                    else if (flag_plussign)
                    {
                        prefix = '+';
                    }
                    else if (flag_blanksign)
                    {
                        prefix = ' ';
                    }
                    else
                    {
                        prefix = 0;
                    }
                }
                else
                {
                    if (longvalue < 0)
                    {
                        longvalue = -longvalue;
                        /* Ticket 1433-003 */
                        if (longvalue < 0)
                        {
                            /* Overflow */
                            longvalue = 0x7FFFFFFFFFFFFFFF;
                        }
                    }
                    prefix = 0;
                }
                if (flag_zeropad && precision < width - (prefix != 0))
                {
                    precision = width - (prefix != 0);
                }
                bufpt = &buf[SXFMT_BUFSIZ - 1];
                {
                    // Use registers for speed
                    register char* cset;
                    register int base;
                    cset = infop->charset;
                    base = infop->base;
                    do
                    {
                        // Convert to ascii
                        *(--bufpt) = cset[longvalue % base];
                        longvalue = longvalue / base;
                    } while (longvalue > 0);
                }
                length = &buf[SXFMT_BUFSIZ - 1] - bufpt;
                for (idx = precision - length; idx > 0; idx--)
                {
                    // Zero pad
                    *(--bufpt) = '0';
                }
                if (prefix)
                {
                    // Add sign
                    *(--bufpt) = prefix;
                }
                if (flag_alternateform && infop->prefix)
                {
                    // Add "0" or "0x"
                    char* pre, x;
                    pre = infop->prefix;
                    if (*bufpt != pre[0])
                    {
                        for (pre = infop->prefix; (x = (*pre)) != 0; pre++)
                        {
                            *(--bufpt) = x;
                        }
                    }
                }
                length = &buf[SXFMT_BUFSIZ - 1] - bufpt;
                break;
            }
            case SXFMT_FLOAT:
            case SXFMT_EXP:
            case SXFMT_GENERIC:
            {
#ifndef SX_OMIT_FLOATINGPOINT
                realvalue = va_arg(ap, double);
                if (precision < 0)
                {
                    // Set default precision
                    precision = 6;
                }
                if (precision > SXFMT_BUFSIZ - 40)
                {
                    precision = SXFMT_BUFSIZ - 40;
                }
                if (realvalue < 0)
                {
                    realvalue = -realvalue;
                    prefix = '-';
                }
                else
                {
                    if (flag_plussign)
                    {
                        prefix = '+';
                    }
                    else if (flag_blanksign)
                    {
                        prefix = ' ';
                    }
                    else
                    {
                        prefix = 0;
                    }
                }
                if (infop->type == SXFMT_GENERIC && precision > 0)
                {
                    precision--;
                }
                rounder = 0;
#if 0
                // Rounding works like BSD when the constant 0.4999 is used. Weird!
                for (idx = precision, rounder = 0.4999; idx > 0; idx--)
#else
                // It makes more sense to use 0.5.
                for (idx = precision, rounder = 0.5; idx > 0; idx--)
#endif
                {
                    rounder *= 0.1;
                }
                if (infop->type == SXFMT_FLOAT)
                {
                    realvalue += rounder;
                }
                /* Normalize realvalue to within 10 > realvalue >= 1 */
                exp = 0;
                if (realvalue > 0)
                {
                    while (realvalue >= 1e8 && exp <= 350)
                    {
                        realvalue *= 1e-8;
                        exp += 8;
                    }
                    while (realvalue >= 10 && exp <= 350)
                    {
                        realvalue *= 0.1;
                        exp++;
                    }
                    while (realvalue < 1e-8 && exp >= -350)
                    {
                        realvalue *= 1e8;
                        exp -= 8;
                    }
                    while (realvalue < 1 && exp >= -350)
                    {
                        realvalue *= 10;
                        exp--;
                    }
                    if (exp > 350 || exp < -350)
                    {
                        bufpt = "NaN";
                        length = 3;
                        break;
                    }
                }
                bufpt = buf;
                // If the field type is etGENERIC, then convert to either etEXP or etFLOAT, as appropriate.
                flag_exp = xtype == SXFMT_EXP;
                if (xtype != SXFMT_FLOAT)
                {
                    realvalue += rounder;
                    if (realvalue >= 10)
                    {
                        realvalue *= 0.1;
                        exp++;
                    }
                }
                if (xtype == SXFMT_GENERIC)
                {
                    flag_rtz = !flag_alternateform;
                    if (exp < -4 || exp > precision)
                    {
                        xtype = SXFMT_EXP;
                    }
                    else
                    {
                        precision = precision - exp;
                        xtype = SXFMT_FLOAT;
                    }
                }
                else
                {
                    flag_rtz = 0;
                }
                // The "exp+precision" test causes output to be of type etEXP if the precision is too large to fit in buf[].
                nsd = 0;
                if (xtype == SXFMT_FLOAT && exp + precision < SXFMT_BUFSIZ - 30)
                {
                    flag_dp = (precision > 0 || flag_alternateform);
                    if (prefix)
                    {
                        // Sign
                        *(bufpt++) = prefix;
                    }
                    if (exp < 0)
                    {
                        // Digits before "."
                        *(bufpt++) = '0';
                    }
                    else
                    {
                        for (; exp >= 0; exp--)
                        {
                            *(bufpt++) = (char)getdigit(&realvalue, &nsd);
                        }
                    }
                    if (flag_dp)
                    {
                        // The decimal point
                        *(bufpt++) = '.';
                    }
                    for (exp++; exp < 0 && precision > 0; precision--, exp++)
                    {
                        *(bufpt++) = '0';
                    }
                    while ((precision--) > 0)
                    {
                        *(bufpt++) = (char)getdigit(&realvalue, &nsd);
                    }
                    // Null terminate
                    *(bufpt--) = 0;
                    if (flag_rtz && flag_dp)
                    {
                        // Remove trailing zeros and "."
                        while (bufpt >= buf && *bufpt == '0') *(bufpt--) = 0;
                        if (bufpt >= buf && *bufpt == '.')
                        {
                            *(bufpt--) = 0;
                        }
                    }
                    // point to next free slot
                    bufpt++;
                }
                else
                {
                    // etEXP or etGENERIC
                    flag_dp = (precision > 0 || flag_alternateform);
                    if (prefix)
                    {
                        // Sign
                        *(bufpt++) = prefix;
                    }
                    // First digit
                    *(bufpt++) = (char)getdigit(&realvalue, &nsd);
                    if (flag_dp)
                    {
                        // Decimal point
                        *(bufpt++) = '.';
                    }
                    while ((precision--) > 0)
                    {
                        *(bufpt++) = (char)getdigit(&realvalue, &nsd);
                    }
                    // Point to last digit
                    bufpt--;
                    if (flag_rtz && flag_dp)
                    {
                        // Remove tail zeros
                        while (bufpt >= buf && *bufpt == '0')
                        {
                            *(bufpt--) = 0;
                        }
                        if (bufpt >= buf && *bufpt == '.')
                        {
                            *(bufpt--) = 0;
                        }
                    }
                    // Point to next free slot
                    bufpt++;
                    if (exp || flag_exp)
                    {
                        *(bufpt++) = infop->charset[0];
                        if (exp < 0)
                        {
                            // Sign of exp
                            *(bufpt++) = '-';
                            exp = -exp;
                        }
                        else
                        {
                            *(bufpt++) = '+';
                        }
                        if (exp >= 100)
                        {
                            // 100's digit
                            *(bufpt++) = (char)((exp / 100) + '0');
                            exp %= 100;
                        }
                        // 10's digit
                        *(bufpt++) = (char)(exp / 10 + '0');
                        // 1's digit
                        *(bufpt++) = (char)(exp % 10 + '0');
                    }
                }
                // The converted number is in buf[] and zero terminated.Output it.
                // Note that the number is in the usual order, not reversed as with integer conversions.
                length = bufpt - buf;
                bufpt = buf;

                // Special case: Add leading zeros if the flag_zeropad flag is set and we are not left justified.
                if (flag_zeropad && !flag_leftjustify && length < width)
                {
                    int nPad = width - length;
                    for (int i = width; i >= nPad; i--)
                    {
                        bufpt[i] = bufpt[i - nPad];
                    }
                    int i = prefix != 0 ? 1 : 0;
                    while (nPad--)
                    {
                        bufpt[i++] = '0';
                    }
                    length = width;
                }
#else
                bufpt = " ";
                length = (int)sizeof(" ") - 1;
#endif // SX_OMIT_FLOATINGPOINT
                break;
            }
            case SXFMT_SIZE:
            {
                int* pSize = va_arg(ap, int *);
                *pSize = ((SyFmtConsumer*)pUserData)->nLen;
                length = width = 0;
                break;
            }
            case SXFMT_PERCENT:
            {
                buf[0] = '%';
                bufpt = buf;
                length = 1;
                break;
            }
            case SXFMT_CHARX:
            {
                c = va_arg(ap, int);
                buf[0] = (char)c;
                // Limit the precision to prevent overflowing buf[] during conversion.
                if (precision > SXFMT_BUFSIZ - 40)
                {
                    precision = SXFMT_BUFSIZ - 40;
                }
                if (precision >= 0)
                {
                    for (idx = 1; idx < precision; idx++)
                    {
                        buf[idx] = (char)c;
                    }
                    length = precision;
                }
                else
                {
                    length = 1;
                }
                bufpt = buf;
                break;
            }
            case SXFMT_STRING:
            {
                bufpt = va_arg(ap, char*);
                if (bufpt == 0)
                {
                    bufpt = " ";
                    length = (int)sizeof(" ") - 1;
                    break;
                }
                length = precision;
                if (precision < 0)
                {
                    // Symisc extension
                    length = (int)SyStrlen(bufpt);
                }
                if (precision >= 0 && precision < length)
                {
                    length = precision;
                }
                break;
            }
            case SXFMT_RAWSTR:
            {
                // Symisc extension
                SyString* pStr = va_arg(ap, SyString *);
                if (pStr == 0 || pStr->zString == 0)
                {
                    bufpt = " ";
                    length = (int)sizeof(char);
                    break;
                }
                bufpt = (char*)pStr->zString;
                length = (int)pStr->nByte;
                break;
            }
            case SXFMT_ERROR:
            {
                buf[0] = '?';
                bufpt = buf;
                length = (int)sizeof(char);
                if (c == 0)
                {
                    zFormat--;
                }
                break;
            }
                // End switch over the format type
        }
        // The text of the conversion is pointed to by "bufpt" and is "length" characters long.
        // The field width is "width". Do the output.
        if (flag_leftjustify == 0)
        {
            register int nspace = width - length;
            if (nspace > 0)
            {
                while (nspace >= etSPACESIZE)
                {
                    rc = xConsumer(spaces, etSPACESIZE, pUserData);
                    if (rc != SXRET_OK)
                    {
                        // Consumer routine request an operation abort
                        return SXERR_ABORT;
                    }
                    nspace -= etSPACESIZE;
                }
                if (nspace > 0)
                {
                    rc = xConsumer(spaces, (unsigned int)nspace, pUserData);
                    if (rc != SXRET_OK)
                    {
                        // Consumer routine request an operation abort
                        return SXERR_ABORT;
                    }
                }
            }
        }
        if (length > 0)
        {
            rc = xConsumer(bufpt, (unsigned int)length, pUserData);
            if (rc != SXRET_OK)
            {
                // Consumer routine request an operation abort
                return SXERR_ABORT;
            }
        }
        if (flag_leftjustify)
        {
            register int nspace;
            nspace = width - length;
            if (nspace > 0)
            {
                while (nspace >= etSPACESIZE)
                {
                    rc = xConsumer(spaces, etSPACESIZE, pUserData);
                    if (rc != SXRET_OK)
                    {
                        // Consumer routine request an operation abort
                        return SXERR_ABORT;
                    }
                    nspace -= etSPACESIZE;
                }
                if (nspace > 0)
                {
                    rc = xConsumer(spaces, (unsigned int)nspace, pUserData);
                    if (rc != SXRET_OK)
                    {
                        // Consumer routine request an operation abort
                        return SXERR_ABORT;
                    }
                }
            }
        }
    }
    // End for loop over the format string
    return errorflag ? SXERR_FORMAT : SXRET_OK;
}

static sxi32 FormatConsumer(const void* pSrc, unsigned int nLen, void* pData)
{
    SyFmtConsumer* pConsumer = (SyFmtConsumer*)pData;
    sxi32 rc = SXERR_ABORT;
    switch (pConsumer->nType)
    {
        case SXFMT_CONS_PROC:
        {
            /* User callback */
            rc = pConsumer->uConsumer.sFunc.xUserConsumer(pSrc, nLen, pConsumer->uConsumer.sFunc.pUserData);
            break;
        }
        case SXFMT_CONS_BLOB:
        {
            /* Blob consumer */
            rc = SyBlobAppend(pConsumer->uConsumer.pBlob, pSrc, (sxu32)
                nLen);
            break;
        }
        default:
            /* Unknown consumer */
            break;
    }

    /* Update total number of bytes consumed so far */
    pConsumer->nLen += nLen;
    pConsumer->rc = rc;

    return rc;
}

static sxi32 FormatMount(
    sxi32 nType,
    void* pConsumer,
    ProcConsumer xUserCons,
    void* pUserData,
    sxu32* pOutLen,
    const char* zFormat,
    va_list ap)
{
    SyFmtConsumer sCons;
    sCons.nType = nType;
    sCons.rc = SXRET_OK;
    sCons.nLen = 0;

    if (pOutLen)
    {
        *pOutLen = 0;
    }
    switch (nType)
    {
        case SXFMT_CONS_PROC:
        {
#if defined(UNTRUST)
            if (xUserCons == 0)
            {
                return SXERR_EMPTY;
            }
#endif
            sCons.uConsumer.sFunc.xUserConsumer = xUserCons;
            sCons.uConsumer.sFunc.pUserData = pUserData;
            break;
        }
        case SXFMT_CONS_BLOB:
        {
            sCons.uConsumer.pBlob = (SyBlob*)pConsumer;
            break;
        }
        default:
            return SXERR_UNKNOWN;
    }

    InternFormat(FormatConsumer, &sCons, zFormat, ap);
    if (pOutLen)
    {
        *pOutLen = sCons.nLen;
    }

    return sCons.rc;
}

PH7_PRIVATE sxi32 SyProcFormat(ProcConsumer xConsumer, void* pData, const char* zFormat, ...)
{
#if defined(UNTRUST)
    if (SX_EMPTY_STR(zFormat))
    {
        return SXERR_EMPTY;
    }
#endif

    va_list ap;
    va_start(ap, zFormat);
    sxi32 rc = FormatMount(SXFMT_CONS_PROC, 0, xConsumer, pData, 0, zFormat, ap);
    va_end(ap);

    return rc;
}

PH7_PRIVATE sxu32 SyBlobFormat(SyBlob* pBlob, const char* zFormat, ...)
{
#if defined(UNTRUST)
    if (SX_EMPTY_STR(zFormat))
    {
        return 0;
    }
#endif

    va_list ap;
    va_start(ap, zFormat);
    sxu32 n = 0;
    FormatMount(SXFMT_CONS_BLOB, &(*pBlob), 0, 0, &n, zFormat, ap);
    va_end(ap);

    return n;
}

PH7_PRIVATE sxu32 SyBlobFormatAp(SyBlob* pBlob, const char* zFormat, va_list ap)
{
#if defined(UNTRUST)
    if (SX_EMPTY_STR(zFormat))
    {
        return 0;
    }
#endif

    sxu32 n = 0;
    FormatMount(SXFMT_CONS_BLOB, &(*pBlob), 0, 0, &n, zFormat, ap);

    return n;
}

PH7_PRIVATE sxu32 SyBufferFormat(char* zBuf, sxu32 nLen, const char* zFormat, ...)
{
#if defined(UNTRUST)
    if (SX_EMPTY_STR(zFormat))
    {
        return 0;
    }
#endif

    SyBlob sBlob;
    if (SXRET_OK != SyBlobInitFromBuf(&sBlob, zBuf, nLen - 1))
    {
        return 0;
    }

    va_list ap;
    va_start(ap, zFormat);
    FormatMount(SXFMT_CONS_BLOB, &sBlob, 0, 0, 0, zFormat, ap);
    va_end(ap);
    sxu32 n = SyBlobLength(&sBlob);
    /* Append the null terminator */
    sBlob.mByte++;
    SyBlobAppend(&sBlob, "\0", sizeof(char));

    return n;
}

#ifndef PH7_DISABLE_BUILTIN_FUNC
/*
* Symisc XML Parser Engine (UTF-8) SAX(Event Driven) API
* @author Mrad Chems Eddine <chm@symisc.net>
* @started 08/03/2010 21:32 FreeBSD
* @finished    07/04/2010 23:24 Win32[VS8]
*/
/*
 * An XML raw text,CDATA,tag name is parsed out and stored
 * in an instance of the following structure.
 */
typedef struct SyXMLRawStrNS SyXMLRawStrNS;
struct SyXMLRawStrNS
{
    /* Public field [Must match the SyXMLRawStr fields ] */
    const char* zString; /* Raw text [UTF-8 ENCODED EXCEPT CDATA] [NOT NULL TERMINATED] */
    sxu32 nByte; /* Text length */
    sxu32 nLine; /* Line number this text occurs */
    /* Private fields */
    SySet sNSset; /* Namespace entries */
};
/*
 * Lexer token codes
 * The following set of constants are the token value recognized
 * by the lexer when processing XML input.
 */
#define SXML_TOK_INVALID    0xFFFF /* Invalid Token */
#define SXML_TOK_COMMENT    0x01   /* Comment */
#define SXML_TOK_PI            0x02   /* Processing instruction */
#define SXML_TOK_DOCTYPE    0x04   /* Doctype directive */
#define SXML_TOK_RAW        0x08   /* Raw text */
#define SXML_TOK_START_TAG    0x10   /* Starting tag */
#define SXML_TOK_CDATA        0x20   /* CDATA */
#define SXML_TOK_END_TAG    0x40   /* Ending tag */
#define SXML_TOK_START_END    0x80   /* Tag */
#define SXML_TOK_SPACE        0x100  /* Spaces (including new lines) */
#define IS_XML_DIRTY(c) \
    ( c == '<' || c == '$'|| c == '"' || c == '\''|| c == '&'|| c == '(' || c == ')' || c == '*' ||\
    c == '%'  || c == '#' || c == '|' || c == '/'|| c == '~' || c == '{' || c == '}' ||\
    c == '['  || c == ']' || c == '\\'|| c == ';'||c == '^'  || c == '`' )

/**
 * Tokenize an entire XML input.
 */
static sxi32 XML_Tokenize(
    SyStream* pStream,
    SyToken* pToken,
    void* pUserData,
    void* pUnused2)
{
    SyXMLParser* pParse = (SyXMLParser*)pUserData;
    sxi32 rc;
    int c;
    /* Jump leading white spaces */
    while (pStream->zText < pStream->zEnd &&
           pStream->zText[0] < 0xc0 &&
           SyisSpace(pStream->zText[0]))
    {
        /* Advance the stream cursor */
        if (pStream->zText[0] == '\n')
        {
            /* Increment line counter */
            pStream->nLine++;
        }
        pStream->zText++;
    }
    if (pStream->zText >= pStream->zEnd)
    {
        SXUNUSED(pUnused2);
        /* End of input reached */
        return SXERR_EOF;
    }
    /* Record token starting position and line */
    pToken->nLine = pStream->nLine;
    pToken->pUserData = 0;
    SyString* pStr = &pToken->sData;
    SyStringInitFromBuf(pStr, pStream->zText, 0);
    /* Extract the current token */
    c = pStream->zText[0];
    if (c == '<')
    {
        pStream->zText++;
        pStr->zString++;
        if (pStream->zText >= pStream->zEnd)
        {
            if (pParse->xError)
            {
                rc = pParse->xError(
                    "Illegal syntax,expecting valid start name character",
                    SXML_ERROR_SYNTAX,
                    pToken,
                    pParse->pUserData);

                if (rc == SXERR_ABORT)
                {
                    return SXERR_ABORT;
                }
            }
            /* End of input reached */
            return SXERR_EOF;
        }
        c = pStream->zText[0];
        if (c == '?')
        {
            /* Processing instruction */
            pStream->zText++;
            pStr->zString++;
            pToken->nType = SXML_TOK_PI;
            while (XLEX_IN_LEN(pStream) >= sizeof("?>") - 1 &&
                   SyMemcmp((const void*)pStream->zText, "?>", sizeof("?>") - 1) != 0)
            {
                if (pStream->zText[0] == '\n')
                {
                    /* Increment line counter */
                    pStream->nLine++;
                }
                pStream->zText++;
            }
            /* Record token length */
            pStr->nByte = (sxu32)((const char*)pStream->zText - pStr->zString);
            if (XLEX_IN_LEN(pStream) < sizeof("?>") - 1)
            {
                if (pParse->xError)
                {
                    rc = pParse->xError(
                        "End of input found,but processing instruction was not found",
                        SXML_ERROR_UNCLOSED_TOKEN,
                        pToken,
                        pParse->pUserData);

                    if (rc == SXERR_ABORT)
                    {
                        return SXERR_ABORT;
                    }
                }
                return SXERR_EOF;
            }
            pStream->zText += sizeof("?>") - 1;
        }
        else if (c == '!')
        {
            pStream->zText++;
            if (XLEX_IN_LEN(pStream) >= sizeof("--") - 1 &&
                pStream->zText[0] == '-' &&
                pStream->zText[1] == '-')
            {
                /* Comment */
                pStream->zText += sizeof("--") - 1;
                while (XLEX_IN_LEN(pStream) >= sizeof("-->") - 1 &&
                       SyMemcmp((const void*)pStream->zText, "-->", sizeof("-->") - 1) != 0)
                {
                    if (pStream->zText[0] == '\n')
                    {
                        /* Increment line counter */
                        pStream->nLine++;
                    }
                    pStream->zText++;
                }
                pStream->zText += sizeof("-->") - 1;
                /* Tell the lexer to ignore this token */
                return SXERR_CONTINUE;
            }
            if (XLEX_IN_LEN(pStream) >= sizeof("[CDATA[") - 1 &&
                SyMemcmp((const void*)pStream->zText, "[CDATA[", sizeof("[CDATA[") - 1) == 0)
            {
                /* CDATA */
                pStream->zText += sizeof("[CDATA[") - 1;
                pStr->zString = (const char*)pStream->zText;
                while (XLEX_IN_LEN(pStream) >= sizeof("]]>") - 1 &&
                       SyMemcmp((const void*)pStream->zText, "]]>", sizeof("]]>") - 1) != 0)
                {
                    if (pStream->zText[0] == '\n')
                    {
                        /* Increment line counter */
                        pStream->nLine++;
                    }
                    pStream->zText++;
                }
                /* Record token type and length */
                pStr->nByte = (sxu32)((const char*)pStream->zText - pStr->zString);
                pToken->nType = SXML_TOK_CDATA;
                if (XLEX_IN_LEN(pStream) < sizeof("]]>") - 1)
                {
                    if (pParse->xError)
                    {
                        rc = pParse->xError(
                            "End of input found,but ]]> was not found",
                            SXML_ERROR_UNCLOSED_TOKEN,
                            pToken,
                            pParse->pUserData);

                        if (rc == SXERR_ABORT)
                        {
                            return SXERR_ABORT;
                        }
                    }
                    return SXERR_EOF;
                }
                pStream->zText += sizeof("]]>") - 1;

                return SXRET_OK;
            }
            if (XLEX_IN_LEN(pStream) >= sizeof("DOCTYPE") - 1 &&
                SyMemcmp((const void*)pStream->zText, "DOCTYPE", sizeof("DOCTYPE") - 1) == 0)
            {
                SyString sDelim = {">", sizeof(char)}; /* Default delimiter */
                int c = 0;
                /* DOCTYPE */
                pStream->zText += sizeof("DOCTYPE") - 1;
                pStr->zString = (const char*)pStream->zText;
                /* Check for element declaration */
                while (pStream->zText < pStream->zEnd && pStream->zText[0] != '\n')
                {
                    if (pStream->zText[0] >= 0xc0 || !SyisSpace(pStream->zText[0]))
                    {
                        c = pStream->zText[0];
                        if (c == '>')
                        {
                            break;
                        }
                    }
                    pStream->zText++;
                }
                if (c == '[')
                {
                    /* Change the delimiter */
                    SyStringInitFromBuf(&sDelim, "]>", sizeof("]>") - 1);
                }
                if (c != '>')
                {
                    while (XLEX_IN_LEN(pStream) >= sDelim.nByte &&
                           SyMemcmp((const void*)pStream->zText, sDelim.zString, sDelim.nByte) != 0)
                    {
                        if (pStream->zText[0] == '\n')
                        {
                            /* Increment line counter */
                            pStream->nLine++;
                        }
                        pStream->zText++;
                    }
                }
                /* Record token type and length */
                pStr->nByte = (sxu32)((const char*)pStream->zText - pStr->zString);
                pToken->nType = SXML_TOK_DOCTYPE;
                if (XLEX_IN_LEN(pStream) < sDelim.nByte)
                {
                    if (pParse->xError)
                    {
                        rc = pParse->xError(
                            "End of input found,but ]> or > was not found",
                            SXML_ERROR_UNCLOSED_TOKEN,
                            pToken,
                            pParse->pUserData);

                        if (rc == SXERR_ABORT)
                        {
                            return SXERR_ABORT;
                        }
                    }
                    return SXERR_EOF;
                }
                pStream->zText += sDelim.nByte;
                return SXRET_OK;
            }
        }
        else
        {
            int c;
            c = pStream->zText[0];
            rc = SXRET_OK;
            pToken->nType = SXML_TOK_START_TAG;
            if (c == '/')
            {
                /* End tag */
                pToken->nType = SXML_TOK_END_TAG;
                pStream->zText++;
                pStr->zString++;
                if (pStream->zText >= pStream->zEnd)
                {
                    if (pParse->xError)
                    {
                        rc = pParse->xError(
                            "Illegal syntax,expecting valid start name character",
                            SXML_ERROR_SYNTAX,
                            pToken,
                            pParse->pUserData);

                        if (rc == SXERR_ABORT)
                        {
                            return SXERR_ABORT;
                        }
                    }
                    return SXERR_EOF;
                }
                c = pStream->zText[0];
            }
            if (c == '>')
            {
                /*<>*/
                if (pParse->xError)
                {
                    rc = pParse->xError(
                        "Illegal syntax,expecting valid start name character",
                        SXML_ERROR_SYNTAX,
                        pToken,
                        pParse->pUserData);

                    if (rc == SXERR_ABORT)
                    {
                        return SXERR_ABORT;
                    }
                }
                /* Ignore the token */
                return SXERR_CONTINUE;
            }
            if (c < 0xc0 && (SyisSpace(c) || SyisDigit(c) || c == '.' || c == '-' || IS_XML_DIRTY(c)))
            {
                if (pParse->xError)
                {
                    rc = pParse->xError(
                        "Illegal syntax,expecting valid start name character",
                        SXML_ERROR_SYNTAX,
                        pToken,
                        pParse->pUserData);

                    if (rc == SXERR_ABORT)
                    {
                        return SXERR_ABORT;
                    }
                }
                rc = SXERR_INVALID;
            }
            pStream->zText++;
            /* Delimit the tag */
            while (pStream->zText < pStream->zEnd && pStream->zText[0] != '>')
            {
                c = pStream->zText[0];
                if (c >= 0xc0)
                {
                    /* UTF-8 stream */
                    pStream->zText++;
                    SX_JMP_UTF8(pStream->zText, pStream->zEnd);
                }
                else
                {
                    if (c == '/' && &pStream->zText[1] < pStream->zEnd && pStream->zText[1] == '>')
                    {
                        pStream->zText++;
                        if (pToken->nType != SXML_TOK_START_TAG)
                        {
                            if (pParse->xError)
                            {
                                rc = pParse->xError(
                                    "Unexpected closing tag,expecting '>'",
                                    SXML_ERROR_SYNTAX,
                                    pToken,
                                    pParse->pUserData);
                                if (rc == SXERR_ABORT)
                                {
                                    return SXERR_ABORT;
                                }
                            }
                            /* Ignore the token */
                            rc = SXERR_INVALID;
                        }
                        else
                        {
                            pToken->nType = SXML_TOK_START_END;
                        }
                        break;
                    }
                    if (pStream->zText[0] == '\n')
                    {
                        /* Increment line counter */
                        pStream->nLine++;
                    }
                    /* Advance the stream cursor */
                    pStream->zText++;
                }
            }
            if (rc != SXRET_OK)
            {
                /* Tell the lexer to ignore this token */
                return SXERR_CONTINUE;
            }
            /* Record token length */
            pStr->nByte = (sxu32)((const char*)pStream->zText - pStr->zString);
            if (pToken->nType == SXML_TOK_START_END && pStr->nByte > 0)
            {
                pStr->nByte -= sizeof(char);
            }
            if (pStream->zText < pStream->zEnd)
            {
                pStream->zText++;
            }
            else
            {
                if (pParse->xError)
                {
                    rc = pParse->xError(
                        "End of input found,but closing tag '>' was not found",
                        SXML_ERROR_UNCLOSED_TOKEN,
                        pToken,
                        pParse->pUserData);

                    if (rc == SXERR_ABORT)
                    {
                        return SXERR_ABORT;
                    }
                }
            }
        }
    }
    else
    {
        /* Raw input */
        while (pStream->zText < pStream->zEnd)
        {
            c = pStream->zText[0];
            if (c < 0xc0)
            {
                if (c == '<')
                {
                    break;
                }
                else if (c == '\n')
                {
                    /* Increment line counter */
                    pStream->nLine++;
                }
                /* Advance the stream cursor */
                pStream->zText++;
            }
            else
            {
                /* UTF-8 stream */
                pStream->zText++;
                SX_JMP_UTF8(pStream->zText, pStream->zEnd);
            }
        }
        /* Record token type,length */
        pToken->nType = SXML_TOK_RAW;
        pStr->nByte = (sxu32)((const char*)pStream->zText - pStr->zString);
    }

    // Return to the lexer
    return SXRET_OK;
}

static int XMLCheckDuplicateAttr(SyXMLRawStr* aSet, sxu32 nEntry, SyXMLRawStr* pEntry)
{
    for (sxu32 n = 0; n < nEntry; n += 2)
    {
        SyXMLRawStr* pAttr = &aSet[n];
        if (pAttr->nByte == pEntry->nByte &&
            SyMemcmp(pAttr->zString, pEntry->zString, pEntry->nByte) == 0)
        {
            /* Attribute found */
            return 1;
        }
    }

    // No duplicates
    return 0;
}

static sxi32 XMLProcessNamesSpace(
    SyXMLParser* pParse,
    SyXMLRawStrNS* pTag,
    SyToken* pToken,
    SySet* pAttr)
{
    sxi32 rc;
    // Namespace prefix/URI
    // Extract the URI first
    SyXMLRawStr* pUri = (SyXMLRawStr*)SySetPeek(pAttr);
    // Extract the prefix
    SyXMLRawStr* pPrefix = (SyXMLRawStr*)SySetAt(pAttr, SySetUsed(pAttr) - 2);
    // Prefix name
    if (pPrefix->nByte == sizeof("xmlns") - 1)
    {
        /* Default namespace */
        pPrefix->nByte = 0;
        // Empty string
        pPrefix->zString = "";
    }
    else
    {
        pPrefix->nByte -= sizeof("xmlns") - 1;
        pPrefix->zString += sizeof("xmlns") - 1;
        if (pPrefix->zString[0] != ':')
        {
            return SXRET_OK;
        }
        pPrefix->nByte--;
        pPrefix->zString++;
        if (pPrefix->nByte < 1)
        {
            if (pParse->xError)
            {
                rc = pParse->xError(
                    "Invalid namespace name",
                    SXML_ERROR_SYNTAX,
                    pToken,
                    pParse->pUserData);

                if (rc == SXERR_ABORT)
                {
                    return SXERR_ABORT;
                }
            }
            /* POP the last insertred two entries */
            (void)SySetPop(pAttr);
            (void)SySetPop(pAttr);

            return SXERR_SYNTAX;
        }
    }
    /* Invoke the namespace callback if available */
    if (pParse->xNameSpace)
    {
        rc = pParse->xNameSpace(pPrefix, pUri, pParse->pUserData);
        if (rc == SXERR_ABORT)
        {
            /* User callback request an operation abort */
            return SXERR_ABORT;
        }
    }
    /* Duplicate structure */
    SyXMLRawStr* pDup = (SyXMLRawStr*)SyMemBackendAlloc(pParse->pAllocator, sizeof(SyXMLRawStr));
    if (pDup == 0)
    {
        if (pParse->xError)
        {
            pParse->xError(
                "Out of memory",
                SXML_ERROR_NO_MEMORY,
                pToken,
                pParse->pUserData);
        }

        /* Abort processing immediately */
        return SXERR_ABORT;
    }
    // Structure assignment
    *pDup = *pUri;
    /* Save the namespace */
    if (pPrefix->nByte == 0)
    {
        pPrefix->zString = "Default";
        pPrefix->nByte = sizeof("Default") - 1;
    }
    SyHashInsert(&pParse->hns, (const void*)pPrefix->zString, pPrefix->nByte, pDup);
    /* Peek the last inserted entry */
    SyHashEntry* pEntry = SyHashLastEntry(&pParse->hns);
    /* Store in the corresponding tag container*/
    SySetPut(&pTag->sNSset, (const void*)&pEntry);
    /* POP the last insertred two entries */
    (void)SySetPop(pAttr);
    (void)SySetPop(pAttr);

    return SXRET_OK;
}

static sxi32 XMLProcessStartTag(
    SyXMLParser* pParse,
    SyToken* pToken,
    SyXMLRawStrNS* pTag,
    SySet* pAttrSet,
    SySet* pTagStack)
{
    sxi32 rc;
    int c;

    SyString* pIn = &pToken->sData;
    /* Reset the working set */
    SySetReset(pAttrSet);
    /* Delimit the raw tag */
    const char* zIn = pIn->zString;
    const char* zEnd = &zIn[pIn->nByte];
    while (zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]))
    {
        zIn++;
    }
    /* Isolate tag name */
    SyXMLRawStr sEntry;
    sEntry.nLine = pTag->nLine = pToken->nLine;
    const char* zCur = zIn;
    while (zIn < zEnd)
    {
        if ((unsigned char)zIn[0] >= 0xc0)
        {
            /* UTF-8 stream */
            zIn++;
            SX_JMP_UTF8(zIn, zEnd);
        }
        else if (SyisSpace(zIn[0]))
        {
            break;
        }
        else
        {
            if (IS_XML_DIRTY(zIn[0]))
            {
                if (pParse->xError)
                {
                    rc = pParse->xError(
                        "Illegal character in XML name",
                        SXML_ERROR_SYNTAX,
                        pToken,
                        pParse->pUserData);

                    if (rc == SXERR_ABORT)
                    {
                        return SXERR_ABORT;
                    }
                }
            }
            zIn++;
        }
    }
    if (zCur >= zIn)
    {
        if (pParse->xError)
        {
            rc = pParse->xError(
                "Invalid XML name",
                SXML_ERROR_SYNTAX,
                pToken,
                pParse->pUserData);

            if (rc == SXERR_ABORT)
            {
                return SXERR_ABORT;
            }
        }

        return SXERR_SYNTAX;
    }
    pTag->zString = zCur;
    pTag->nByte = (sxu32)(zIn - zCur);
    /* Process tag attribute */
    for (;;)
    {
        int is_ns = 0;
        while (zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]))
        {
            zIn++;
        }
        if (zIn >= zEnd)
        {
            break;
        }
        zCur = zIn;
        while (zIn < zEnd && zIn[0] != '=')
        {
            if ((unsigned char)zIn[0] >= 0xc0)
            {
                /* UTF-8 stream */
                zIn++;
                SX_JMP_UTF8(zIn, zEnd);
            }
            else if (SyisSpace(zIn[0]))
            {
                break;
            }
            else
            {
                zIn++;
            }
        }
        if (zCur >= zIn)
        {
            if (pParse->xError)
            {
                rc = pParse->xError(
                    "Missing attribute name",
                    SXML_ERROR_SYNTAX,
                    pToken,
                    pParse->pUserData);

                if (rc == SXERR_ABORT)
                {
                    return SXERR_ABORT;
                }
            }
            return SXERR_SYNTAX;
        }
        /* Store attribute name */
        sEntry.zString = zCur;
        sEntry.nByte = (sxu32)(zIn - zCur);
        if ((pParse->nFlags & SXML_ENABLE_NAMESPACE) && sEntry.nByte >= sizeof("xmlns") - 1 &&
            SyMemcmp(sEntry.zString, "xmlns", sizeof("xmlns") - 1) == 0)
        {
            is_ns = 1;
        }
        while (zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]))
        {
            zIn++;
        }
        if (zIn >= zEnd || zIn[0] != '=')
        {
            if (pParse->xError)
            {
                rc = pParse->xError(
                    "Missing attribute value",
                    SXML_ERROR_SYNTAX,
                    pToken,
                    pParse->pUserData);

                if (rc == SXERR_ABORT)
                {
                    return SXERR_ABORT;
                }
            }

            return SXERR_SYNTAX;
        }
        while (sEntry.nByte > 0 && (unsigned char)zCur[sEntry.nByte - 1] < 0xc0 &&
               SyisSpace(zCur[sEntry.nByte - 1]))
        {
            sEntry.nByte--;
        }
        /* Check for duplicates first */
        if (XMLCheckDuplicateAttr((SyXMLRawStr*)SySetBasePtr(pAttrSet), SySetUsed(pAttrSet), &sEntry))
        {
            if (pParse->xError)
            {
                rc = pParse->xError(
                    "Duplicate attribute",
                    SXML_ERROR_DUPLICATE_ATTRIBUTE,
                    pToken,
                    pParse->pUserData);

                if (rc == SXERR_ABORT)
                {
                    return SXERR_ABORT;
                }
            }

            return SXERR_SYNTAX;
        }
        if (SXRET_OK != SySetPut(pAttrSet, (const void*)&sEntry))
        {
            return SXERR_ABORT;
        }
        /* Extract attribute value */
        zIn++; /* Jump the trailing '=' */
        while (zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]))
        {
            zIn++;
        }
        if (zIn >= zEnd)
        {
            if (pParse->xError)
            {
                rc = pParse->xError(
                    "Missing attribute value",
                    SXML_ERROR_SYNTAX,
                    pToken,
                    pParse->pUserData);

                if (rc == SXERR_ABORT)
                {
                    return SXERR_ABORT;
                }
            }
            (void)SySetPop(pAttrSet);

            return SXERR_SYNTAX;
        }
        if (zIn[0] != '\'' && zIn[0] != '"')
        {
            if (pParse->xError)
            {
                rc = pParse->xError(
                    "Missing quotes on attribute value",
                    SXML_ERROR_SYNTAX,
                    pToken,
                    pParse->pUserData);

                if (rc == SXERR_ABORT)
                {
                    return SXERR_ABORT;
                }
            }
            (void)SySetPop(pAttrSet);

            return SXERR_SYNTAX;
        }
        c = zIn[0];
        zIn++;
        zCur = zIn;
        while (zIn < zEnd && zIn[0] != c)
        {
            zIn++;
        }
        if (zIn >= zEnd)
        {
            if (pParse->xError)
            {
                rc = pParse->xError(
                    "Missing quotes on attribute value",
                    SXML_ERROR_SYNTAX,
                    pToken,
                    pParse->pUserData);

                if (rc == SXERR_ABORT)
                {
                    return SXERR_ABORT;
                }
            }
            (void)SySetPop(pAttrSet);

            return SXERR_SYNTAX;
        }
        /* Store attribute value */
        sEntry.zString = zCur;
        sEntry.nByte = (sxu32)(zIn - zCur);
        if (SXRET_OK != SySetPut(pAttrSet, (const void*)&sEntry))
        {
            return SXERR_ABORT;
        }
        zIn++;
        if (is_ns)
        {
            /* Process namespace declaration */
            XMLProcessNamesSpace(pParse, pTag, pToken, pAttrSet);
        }
    }
    /* Store in the tag stack */
    if (pToken->nType == SXML_TOK_START_TAG)
    {
        rc = SySetPut(pTagStack, (const void*)pTag);
    }

    return SXRET_OK;
}

static void XMLExtactPI(
    SyToken* pToken,
    SyXMLRawStr* pTarget,
    SyXMLRawStr* pData,
    int* pXML)
{
    SyString* pIn = &pToken->sData;
    const char* zIn, * zCur, * zEnd;

    pTarget->nLine = pData->nLine = pToken->nLine;
    /* Nullify the entries first */
    pTarget->zString = pData->zString = 0;
    /* Ignore leading and traing white spaces */
    SyStringFullTrim(pIn);
    /* Delimit the raw PI */
    zIn = pIn->zString;
    zEnd = &zIn[pIn->nByte];
    if (pXML)
    {
        *pXML = 0;
    }
    /* Extract the target */
    zCur = zIn;
    while (zIn < zEnd)
    {
        if ((unsigned char)zIn[0] >= 0xc0)
        {
            /* UTF-8 stream */
            zIn++;
            SX_JMP_UTF8(zIn, zEnd);
        }
        else if (SyisSpace(zIn[0]))
        {
            break;
        }
        else
        {
            zIn++;
        }
    }
    if (zIn > zCur)
    {
        pTarget->zString = zCur;
        pTarget->nByte = (sxu32)(zIn - zCur);
        if (pXML && pTarget->nByte == sizeof("xml") - 1 &&
            SyStrnicmp(pTarget->zString, "xml", sizeof("xml") - 1) == 0)
        {
            *pXML = 1;
        }
    }
    /* Extract the PI data  */
    while (zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]))
    {
        zIn++;
    }
    if (zIn < zEnd)
    {
        pData->zString = zIn;
        pData->nByte = (sxu32)(zEnd - zIn);
    }
}

static sxi32 XMLExtractEndTag(SyXMLParser* pParse, SyToken* pToken, SyXMLRawStrNS* pOut)
{
    SyString* pIn = &pToken->sData;
    const char* zEnd = &pIn->zString[pIn->nByte];
    const char* zIn = pIn->zString;

    /* Ignore leading white spaces */
    while (zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]))
    {
        zIn++;
    }
    pOut->nLine = pToken->nLine;
    pOut->zString = zIn;
    pOut->nByte = (sxu32)(zEnd - zIn);

    /* Ignore trailing white spaces */
    while (pOut->nByte > 0 && (unsigned char)pOut->zString[pOut->nByte - 1] < 0xc0 &&
           SyisSpace(pOut->zString[pOut->nByte - 1]))
    {
        pOut->nByte--;
    }
    if (pOut->nByte < 1)
    {
        if (pParse->xError)
        {
            sxi32 rc = pParse->xError(
                "Invalid end tag name",
                SXML_ERROR_INVALID_TOKEN,
                pToken,
                pParse->pUserData);

            if (rc == SXERR_ABORT)
            {
                return SXERR_ABORT;
            }
        }

        return SXERR_SYNTAX;
    }

    return SXRET_OK;
}

static void TokenToXMLString(SyToken* pTok, SyXMLRawStrNS* pOut)
{
    // Remove leading and trailing white spaces first
    SyStringFullTrim(&pTok->sData);
    pOut->zString = SyStringData(&pTok->sData);
    pOut->nByte = SyStringLength(&pTok->sData);
}

static sxi32 XMLExtractNS(SyXMLParser* pParse, SyToken* pToken, SyXMLRawStrNS* pTag, SyXMLRawStr* pnsUri)
{
    SyXMLRawStr* pUri, sPrefix;
    SyHashEntry* pEntry;
    sxu32 nOfft;
    /* Extract a prefix if available */
    sxi32 rc = SyByteFind(pTag->zString, pTag->nByte, ':', &nOfft);
    if (rc != SXRET_OK)
    {
        /* Check if there is a default namespace */
        pEntry = SyHashGet(&pParse->hns, "Default", sizeof("Default") - 1);
        if (pEntry != NULL)
        {
            /* Extract the ns URI */
            pUri = (SyXMLRawStr*)pEntry->pUserData;
            /* Save the ns URI */
            pnsUri->zString = pUri->zString;
            pnsUri->nByte = pUri->nByte;
        }
        return SXRET_OK;
    }
    if (nOfft < 1)
    {
        if (pParse->xError)
        {
            rc = pParse->xError(
                "Empty prefix is not allowed according to XML namespace specification",
                SXML_ERROR_SYNTAX,
                pToken,
                pParse->pUserData);

            if (rc == SXERR_ABORT)
            {
                return SXERR_ABORT;
            }
        }
        return SXERR_SYNTAX;
    }
    sPrefix.zString = pTag->zString;
    sPrefix.nByte = nOfft;
    sPrefix.nLine = pTag->nLine;
    pTag->zString += nOfft + 1;
    pTag->nByte -= nOfft;
    if (pTag->nByte < 1)
    {
        if (pParse->xError)
        {
            rc = pParse->xError(
                "Missing tag name",
                SXML_ERROR_SYNTAX,
                pToken,
                pParse->pUserData);

            if (rc == SXERR_ABORT)
            {
                return SXERR_ABORT;
            }
        }
        return SXERR_SYNTAX;
    }
    /* Check if the prefix is already registered */
    pEntry = SyHashGet(&pParse->hns, sPrefix.zString, sPrefix.nByte);
    if (pEntry == 0)
    {
        if (pParse->xError)
        {
            rc = pParse->xError(
                "Namespace prefix is not defined",
                SXML_ERROR_SYNTAX,
                pToken,
                pParse->pUserData);

            if (rc == SXERR_ABORT)
            {
                return SXERR_ABORT;
            }
        }
        return SXERR_SYNTAX;
    }
    /* Extract the ns URI */
    pUri = (SyXMLRawStr*)pEntry->pUserData;
    /* Save the ns URI */
    pnsUri->zString = pUri->zString;
    pnsUri->nByte = pUri->nByte;

    /* All done */
    return SXRET_OK;
}

static sxi32 XMLnsUnlink(SyXMLParser* pParse, SyXMLRawStrNS* pLast, SyToken* pToken)
{
    /* Release namespace entries */
    SyHashEntry** apEntry = (SyHashEntry**)SySetBasePtr(&pLast->sNSset);
    for (sxu32 n = 0; n < SySetUsed(&pLast->sNSset); ++n)
    {
        SyHashEntry* pEntry = apEntry[n];
        /* Invoke the end namespace declaration callback */
        if (pParse->xNameSpaceEnd && (pParse->nFlags & SXML_ENABLE_NAMESPACE) && pToken)
        {
            SyXMLRawStr sPrefix = {};
            sPrefix.zString = (const char*)pEntry->pKey;
            sPrefix.nByte = pEntry->nKeyLen;
            sPrefix.nLine = pToken->nLine;
            sxi32 rc = pParse->xNameSpaceEnd(&sPrefix, pParse->pUserData);
            if (rc == SXERR_ABORT)
            {
                return SXERR_ABORT;
            }
        }
        void* pUserData = pEntry->pUserData;
        /* Remove from the namespace hashtable */
        SyHashDeleteEntry2(pEntry);
        SyMemBackendFree(pParse->pAllocator, pUserData);
    }
    SySetRelease(&pLast->sNSset);

    return SXRET_OK;
}

/**
 * Process XML tokens.
 */
static sxi32 ProcessXML(SyXMLParser* pParse, SySet* pTagStack, SySet* pWorker)
{
    SySet* pTokenSet = &pParse->sToken;
    SyXMLRawStrNS sEntry;
    SyXMLRawStr sNs;
    SyToken* pToken;
    sxi32 rc;
    /* Initialize fields */
    int bGotTag = 0;
    /* Start processing */
    if (pParse->xStartDoc && (SXERR_ABORT == pParse->xStartDoc(pParse->pUserData)))
    {
        /* User callback request an operation abort */
        return SXERR_ABORT;
    }
    /* Reset the loop cursor */
    SySetResetCursor(pTokenSet);
    /* Extract the current token */
    while (SXRET_OK == (SySetGetNextEntry(&(*pTokenSet), (void**)&pToken)))
    {
        SyZero(&sEntry, sizeof(SyXMLRawStrNS));
        SyZero(&sNs, sizeof(SyXMLRawStr));
        SySetInit(&sEntry.sNSset, pParse->pAllocator, sizeof(SyHashEntry*));
        sEntry.nLine = sNs.nLine = pToken->nLine;

        switch (pToken->nType)
        {
            case SXML_TOK_DOCTYPE:
            {
                if (SySetUsed(pTagStack) > 1 || bGotTag)
                {
                    if (pParse->xError)
                    {
                        rc = pParse->xError(
                            "DOCTYPE must be declared first",
                            SXML_ERROR_MISPLACED_XML_PI,
                            pToken,
                            pParse->pUserData);

                        if (rc == SXERR_ABORT)
                        {
                            return SXERR_ABORT;
                        }
                    }
                    break;
                }
                /* Invoke the supplied callback if any */
                if (pParse->xDoctype)
                {
                    TokenToXMLString(pToken, &sEntry);
                    rc = pParse->xDoctype((SyXMLRawStr*)&sEntry, pParse->pUserData);

                    if (rc == SXERR_ABORT)
                    {
                        return SXERR_ABORT;
                    }
                }
                break;
            }
            case SXML_TOK_CDATA:
            {
                if (SySetUsed(pTagStack) < 1)
                {
                    if (pParse->xError)
                    {
                        rc = pParse->xError(
                            "CDATA without matching tag",
                            SXML_ERROR_TAG_MISMATCH,
                            pToken,
                            pParse->pUserData);

                        if (rc == SXERR_ABORT)
                        {
                            return SXERR_ABORT;
                        }
                    }
                }
                /* Invoke the supplied callback if any */
                if (pParse->xRaw)
                {
                    TokenToXMLString(pToken, &sEntry);
                    rc = pParse->xRaw((SyXMLRawStr*)&sEntry, pParse->pUserData);

                    if (rc == SXERR_ABORT)
                    {
                        return SXERR_ABORT;
                    }
                }
                break;
            }
            case SXML_TOK_PI:
            {
                SyXMLRawStr sTarget, sData;
                int isXML = 0;
                /* Extract the target and data */
                XMLExtactPI(pToken, &sTarget, &sData, &isXML);
                if (isXML && SySetCursor(pTokenSet) - 1 > 0)
                {
                    if (pParse->xError)
                    {
                        rc = pParse->xError(
                            "Unexpected XML declaration. The XML declaration must be the first node in the document",
                            SXML_ERROR_MISPLACED_XML_PI,
                            pToken,
                            pParse->pUserData);

                        if (rc == SXERR_ABORT)
                        {
                            return SXERR_ABORT;
                        }
                    }
                }
                else if (pParse->xPi)
                {
                    /* Invoke the supplied callback*/
                    rc = pParse->xPi(&sTarget, &sData, pParse->pUserData);

                    if (rc == SXERR_ABORT)
                    {
                        return SXERR_ABORT;
                    }
                }
                break;
            }
            case SXML_TOK_RAW:
            {
                if (SySetUsed(pTagStack) < 1)
                {
                    if (pParse->xError)
                    {
                        rc = pParse->xError(
                            "Text (Raw data) without matching tag",
                            SXML_ERROR_TAG_MISMATCH,
                            pToken,
                            pParse->pUserData);

                        if (rc == SXERR_ABORT)
                        {
                            return SXERR_ABORT;
                        }
                    }
                    break;
                }
                /* Invoke the supplied callback if any */
                if (pParse->xRaw)
                {
                    TokenToXMLString(pToken, &sEntry);
                    rc = pParse->xRaw((SyXMLRawStr*)&sEntry, pParse->pUserData);
                    if (rc == SXERR_ABORT)
                    {
                        return SXERR_ABORT;
                    }
                }
                break;
            }
            case SXML_TOK_END_TAG:
            {
                SyXMLRawStrNS* pLast = 0; /* cc warning */
                if (SySetUsed(pTagStack) < 1)
                {
                    if (pParse->xError)
                    {
                        rc = pParse->xError(
                            "Unexpected closing tag",
                            SXML_ERROR_TAG_MISMATCH,
                            pToken,
                            pParse->pUserData);

                        if (rc == SXERR_ABORT)
                        {
                            return SXERR_ABORT;
                        }
                    }
                    break;
                }
                rc = XMLExtractEndTag(pParse, pToken, &sEntry);
                if (rc == SXRET_OK)
                {
                    /* Extract the last inserted entry */
                    pLast = (SyXMLRawStrNS*)SySetPeek(pTagStack);
                    if (pLast == 0 || pLast->nByte != sEntry.nByte ||
                        SyMemcmp(pLast->zString, sEntry.zString, sEntry.nByte) != 0)
                    {
                        if (pParse->xError)
                        {
                            rc = pParse->xError(
                                "Unexpected closing tag",
                                SXML_ERROR_TAG_MISMATCH,
                                pToken,
                                pParse->pUserData);

                            if (rc == SXERR_ABORT)
                            {
                                return SXERR_ABORT;
                            }
                        }
                    }
                    else
                    {
                        /* Invoke the supllied callback if any */
                        if (pParse->xEndTag)
                        {
                            rc = SXRET_OK;
                            if (pParse->nFlags & SXML_ENABLE_NAMESPACE)
                            {
                                /* Extract namespace URI */
                                rc = XMLExtractNS(pParse, pToken, &sEntry, &sNs);
                                if (rc == SXERR_ABORT)
                                {
                                    return SXERR_ABORT;
                                }
                            }
                            if (rc == SXRET_OK)
                            {
                                rc = pParse->xEndTag((SyXMLRawStr*)&sEntry, &sNs, pParse->pUserData);
                                if (rc == SXERR_ABORT)
                                {
                                    return SXERR_ABORT;
                                }
                            }
                        }
                    }
                }
                else if (rc == SXERR_ABORT)
                {
                    return SXERR_ABORT;
                }
                if (pLast)
                {
                    rc = XMLnsUnlink(pParse, pLast, pToken);
                    (void)SySetPop(pTagStack);
                    if (rc == SXERR_ABORT)
                    {
                        return SXERR_ABORT;
                    }
                }
                break;
            }
            case SXML_TOK_START_TAG:
            case SXML_TOK_START_END:
            {
                if (SySetUsed(pTagStack) < 1 && bGotTag)
                {
                    if (pParse->xError)
                    {
                        rc = pParse->xError("XML document cannot contain multiple root level elements documents",
                                            SXML_ERROR_SYNTAX, pToken, pParse->pUserData);
                        if (rc == SXERR_ABORT)
                        {
                            return SXERR_ABORT;
                        }
                    }
                    break;
                }
                bGotTag = 1;
                /* Extract the tag and it's supplied attribute */
                rc = XMLProcessStartTag(pParse, pToken, &sEntry, pWorker, pTagStack);
                if (rc == SXRET_OK)
                {
                    if (pParse->nFlags & SXML_ENABLE_NAMESPACE)
                    {
                        /* Extract namespace URI */
                        rc = XMLExtractNS(pParse, pToken, &sEntry, &sNs);
                    }
                }
                if (rc == SXRET_OK)
                {
                    /* Invoke the supplied callback */
                    if (pParse->xStartTag)
                    {
                        rc = pParse->xStartTag(
                            (SyXMLRawStr*)&sEntry,
                            &sNs,
                            SySetUsed(pWorker),
                            (SyXMLRawStr*)SySetBasePtr(pWorker),
                            pParse->pUserData);
                        if (rc == SXERR_ABORT)
                        {
                            return SXERR_ABORT;
                        }
                    }
                    if (pToken->nType == SXML_TOK_START_END)
                    {
                        if (pParse->xEndTag)
                        {
                            rc = pParse->xEndTag((SyXMLRawStr*)&sEntry, &sNs, pParse->pUserData);
                            if (rc == SXERR_ABORT)
                            {
                                return SXERR_ABORT;
                            }
                        }
                        rc = XMLnsUnlink(pParse, &sEntry, pToken);
                        if (rc == SXERR_ABORT)
                        {
                            return SXERR_ABORT;
                        }
                    }
                }
                else if (rc == SXERR_ABORT)
                {
                    // Abort processing immediately.
                    return SXERR_ABORT;
                }
                break;
            }
            default:
                // Can't happen.
                break;
        }
    }
    if (SySetUsed(pTagStack) > 0 && pParse->xError)
    {
        pParse->xError(
            "Missing closing tag",
            SXML_ERROR_SYNTAX,
            (SyToken*)SySetPeek(&pParse->sToken),
            pParse->pUserData);
    }
    if (pParse->xEndDoc)
    {
        pParse->xEndDoc(pParse->pUserData);
    }

    return SXRET_OK;
}

PH7_PRIVATE sxi32 SyXMLParserInit(
    SyXMLParser* pParser,
    SyMemBackend* pAllocator,
    sxi32 iFlags)
{
    // Zero the structure first
    SyZero(pParser, sizeof(SyXMLParser));

    // Initilaize fields
    SySetInit(&pParser->sToken, pAllocator, sizeof(SyToken));
    SyLexInit(&pParser->sLex, &pParser->sToken, XML_Tokenize, pParser);
    SyHashInit(&pParser->hns, pAllocator, 0, 0);
    pParser->pAllocator = pAllocator;
    pParser->nFlags = iFlags;

    return SXRET_OK;
}

PH7_PRIVATE sxi32 SyXMLParserSetEventHandler(
    SyXMLParser* pParser,
    void* pUserData,
    ProcXMLStartTagHandler xStartTag,
    ProcXMLTextHandler xRaw,
    ProcXMLSyntaxErrorHandler xErr,
    ProcXMLStartDocument xStartDoc,
    ProcXMLEndTagHandler xEndTag,
    ProcXMLPIHandler xPi,
    ProcXMLEndDocument xEndDoc,
    ProcXMLDoctypeHandler xDoctype,
    ProcXMLNameSpaceStart xNameSpace,
    ProcXMLNameSpaceEnd xNameSpaceEnd)
{
    // Install user callbacks
    if (xErr)
    {
        pParser->xError = xErr;
    }
    if (xStartDoc)
    {
        pParser->xStartDoc = xStartDoc;
    }
    if (xStartTag)
    {
        pParser->xStartTag = xStartTag;
    }
    if (xRaw)
    {
        pParser->xRaw = xRaw;
    }
    if (xEndTag)
    {
        pParser->xEndTag = xEndTag;
    }
    if (xPi)
    {
        pParser->xPi = xPi;
    }
    if (xEndDoc)
    {
        pParser->xEndDoc = xEndDoc;
    }
    if (xDoctype)
    {
        pParser->xDoctype = xDoctype;
    }
    if (xNameSpace)
    {
        pParser->xNameSpace = xNameSpace;
    }
    if (xNameSpaceEnd)
    {
        pParser->xNameSpaceEnd = xNameSpaceEnd;
    }
    pParser->pUserData = pUserData;

    return SXRET_OK;
}

/**
 * Process an XML chunk.
 */
PH7_PRIVATE sxi32 SyXMLProcess(SyXMLParser* pParser, const char* zInput, sxu32 nByte)
{
    // Initialize working sets.
    SySet sWorker, sTagStack;
    // Tag container and stack.
    SySetInit(&sWorker, pParser->pAllocator, sizeof(SyXMLRawStr));
    SySetInit(&sTagStack, pParser->pAllocator, sizeof(SyXMLRawStrNS));

    // Tokenize the entire input
    sxi32 rc = SyLexTokenizeInput(&pParser->sLex, zInput, nByte, 0, 0, 0);
    if (rc == SXERR_ABORT)
    {
        // Tokenize callback request an operation abort
        return SXERR_ABORT;
    }

    if (SySetUsed(&pParser->sToken) < 1)
    {
        // Nothing to process [i.e: white spaces]
        rc = SXRET_OK;
    }
    else
    {
        // Process XML Tokens
        rc = ProcessXML(&(*pParser), &sTagStack, &sWorker);
        if (pParser->nFlags & SXML_ENABLE_NAMESPACE)
        {
            if (SySetUsed(&sTagStack) > 0)
            {
                SySetResetCursor(&sTagStack);
                SyXMLRawStrNS* pEntry;
                while (SySetGetNextEntry(&sTagStack, (void**)&pEntry) == SXRET_OK)
                {
                    // Release namespace entries */
                    SyHashEntry** apEntry = (SyHashEntry**)SySetBasePtr(&pEntry->sNSset);
                    for (sxu32 n = 0; n < SySetUsed(&pEntry->sNSset); ++n)
                    {
                        SyMemBackendFree(pParser->pAllocator, apEntry[n]->pUserData);
                    }
                    SySetRelease(&pEntry->sNSset);
                }
            }
        }
    }

    // Clean-up the mess left behind
    SySetRelease(&sWorker);
    SySetRelease(&sTagStack);

    // Processing result
    return rc;
}

PH7_PRIVATE sxi32 SyXMLParserRelease(SyXMLParser* pParser)
{
    SyLexRelease(&pParser->sLex);
    SySetRelease(&pParser->sToken);
    SyHashRelease(&pParser->hns);

    return SXRET_OK;
}

/*
 * Zip File Format:
 *
 * Byte order: Little-endian
 *
 * [Local file header + Compressed data [+ Extended local header]?]*
 * [Central directory]*
 * [End of central directory record]
 *
 * Local file header:*
 * Offset   Length   Contents
 *  0      4 bytes  Local file header signature (0x04034b50)
 *  4      2 bytes  Version needed to extract
 *  6      2 bytes  General purpose bit flag
 *  8      2 bytes  Compression method
 * 10      2 bytes  Last mod file time
 * 12      2 bytes  Last mod file date
 * 14      4 bytes  CRC-32
 * 18      4 bytes  Compressed size (n)
 * 22      4 bytes  Uncompressed size
 * 26      2 bytes  Filename length (f)
 * 28      2 bytes  Extra field length (e)
 * 30     (f)bytes  Filename
 *        (e)bytes  Extra field
 *        (n)bytes  Compressed data
 *
 * Extended local header:*
 * Offset   Length   Contents
 *  0      4 bytes  Extended Local file header signature (0x08074b50)
 *  4      4 bytes  CRC-32
 *  8      4 bytes  Compressed size
 * 12      4 bytes  Uncompressed size
 *
 * Extra field:?(if any)
 * Offset     Length        Contents
 * 0          2 bytes        Header ID (0x001 until 0xfb4a) see extended appnote from Info-zip
 * 2          2 bytes        Data size (g)
 *               (g) bytes    (g) bytes of extra field
 *
 * Central directory:*
 * Offset   Length   Contents
 *  0      4 bytes  Central file header signature (0x02014b50)
 *  4      2 bytes  Version made by
 *  6      2 bytes  Version needed to extract
 *  8      2 bytes  General purpose bit flag
 * 10      2 bytes  Compression method
 * 12      2 bytes  Last mod file time
 * 14      2 bytes  Last mod file date
 * 16      4 bytes  CRC-32
 * 20      4 bytes  Compressed size
 * 24      4 bytes  Uncompressed size
 * 28      2 bytes  Filename length (f)
 * 30      2 bytes  Extra field length (e)
 * 32      2 bytes  File comment length (c)
 * 34      2 bytes  Disk number start
 * 36      2 bytes  Internal file attributes
 * 38      4 bytes  External file attributes
 * 42      4 bytes  Relative offset of local header
 * 46     (f)bytes  Filename
 *        (e)bytes  Extra field
 *        (c)bytes  File comment
 *
 * End of central directory record:
 * Offset   Length   Contents
 *  0      4 bytes  End of central dir signature (0x06054b50)
 *  4      2 bytes  Number of this disk
 *  6      2 bytes  Number of the disk with the start of the central directory
 *  8      2 bytes  Total number of entries in the central dir on this disk
 * 10      2 bytes  Total number of entries in the central dir
 * 12      4 bytes  Size of the central directory
 * 16      4 bytes  Offset of start of central directory with respect to the starting disk number
 * 20      2 bytes  zipfile comment length (c)
 * 22     (c)bytes  zipfile comment
 *
 * compression method: (2 bytes)
 *          0 - The file is stored (no compression)
 *          1 - The file is Shrunk
 *          2 - The file is Reduced with compression factor 1
 *          3 - The file is Reduced with compression factor 2
 *          4 - The file is Reduced with compression factor 3
 *          5 - The file is Reduced with compression factor 4
 *          6 - The file is Imploded
 *          7 - Reserved for Tokenizing compression algorithm
 *          8 - The file is Deflated
 */

#define SXMAKE_ZIP_WORKBUF    (SXU16_HIGH/2)    /* 32KB Initial working buffer size */
#define SXMAKE_ZIP_EXTRACT_VER    0x000a    /* Version needed to extract */
#define SXMAKE_ZIP_VER    0x003    /* Version made by */

#define SXZIP_CENTRAL_MAGIC        0x02014b50
#define SXZIP_END_CENTRAL_MAGIC    0x06054b50
#define SXZIP_LOCAL_MAGIC          0x04034b50
#if 0
#define SXZIP_CRC32_START          0xdebb20e3
#endif

#define SXZIP_LOCAL_HDRSZ          30    // Local header size
#define SXZIP_LOCAL_EXT_HDRZ       16    // Extended local header(footer) size
#define SXZIP_CENTRAL_HDRSZ        46    // Central directory header size
#define SXZIP_END_CENTRAL_HDRSZ    22    // End of central directory header size
#define SXARCHIVE_HASH_SIZE        64    // Starting hash table size(MUST BE POWER OF 2)

static sxi32 SyLittleEndianUnpack32(sxu32* uNB, const unsigned char* buf, sxu32 Len)
{
    if (Len < sizeof(sxu32))
    {
        return SXERR_SHORT;
    }
    *uNB = buf[0] + (buf[1] << 8u) + (buf[2] << 16u) + (buf[3] << 24u);

    return SXRET_OK;
}

static sxi32 SyLittleEndianUnpack16(sxu16* pOut, const unsigned char* zBuf, sxu32 nLen)
{
    if (nLen < sizeof(sxu16))
    {
        return SXERR_SHORT;
    }
    *pOut = zBuf[0] + (zBuf[1] << 8u);

    return SXRET_OK;
}

static sxi32 SyDosTimeFormat(sxu32 nDosDate, Sytm* pOut)
{
    sxu16 nDate;
    sxu16 nTime;
    nDate = nDosDate >> 16u;
    nTime = nDosDate & 0xFFFFu;
    pOut->tm_isdst = 0;
    pOut->tm_year = 1980 + (nDate >> 9u);
    pOut->tm_mon = (nDate % (1u << 9u)) >> 5u;
    pOut->tm_mday = (nDate % (1u << 9u)) & 0x1Fu;
    pOut->tm_hour = nTime >> 11u;
    pOut->tm_min = (nTime % (1u << 11u)) >> 5u;
    pOut->tm_sec = ((nTime % (1u << 11u)) & 0x1Fu) << 1u;

    return SXRET_OK;
}

/**
 * Archive hashtable manager
 */
static sxi32 ArchiveHashGetEntry(SyArchive* pArch, const char* zName, sxu32 nLen, SyArchiveEntry** ppEntry)
{
    SyArchiveEntry* pBucketEntry;
    SyString sEntry;
    sxu32 nHash;

    nHash = pArch->xHash(zName, nLen);
    pBucketEntry = pArch->apHash[nHash & (pArch->nSize - 1)];

    SyStringInitFromBuf(&sEntry, zName, nLen);

    for (;;)
    {
        if (pBucketEntry == 0)
        {
            break;
        }
        if (nHash == pBucketEntry->nHash && pArch->xCmp(&sEntry, &pBucketEntry->sFileName) == 0)
        {
            if (ppEntry != NULL)
            {
                *ppEntry = pBucketEntry;
            }
            return SXRET_OK;
        }
        pBucketEntry = pBucketEntry->pNextHash;
    }

    return SXERR_NOTFOUND;
}

static void ArchiveHashBucketInstall(SyArchiveEntry** apTable, sxu32 nBucket, SyArchiveEntry* pEntry)
{
    pEntry->pNextHash = apTable[nBucket];
    if (apTable[nBucket] != 0)
    {
        apTable[nBucket]->pPrevHash = pEntry;
    }
    apTable[nBucket] = pEntry;
}

static sxi32 ArchiveHashGrowTable(SyArchive* pArch)
{
    sxu32 nNewSize = pArch->nSize * 2;
    // Allocate a new table.
    SyArchiveEntry** apNew = (SyArchiveEntry**)SyMemBackendAlloc(pArch->pAllocator, nNewSize * sizeof(SyArchiveEntry*));
    if (apNew == 0)
    {
        // Not so fatal,simply a performance hit.
        return SXRET_OK;
    }
    SyZero(apNew, nNewSize * sizeof(SyArchiveEntry*));
    // Rehash old entries.
    SyArchiveEntry* pEntry = pArch->pList;
    for (sxu32 n = 0; n < pArch->nLoaded; n++)
    {
        pEntry->pNextHash = pEntry->pPrevHash = 0;
        ArchiveHashBucketInstall(apNew, pEntry->nHash & (nNewSize - 1), pEntry);
        pEntry = pEntry->pNext;
    }
    // Release the old table.
    SyMemBackendFree(pArch->pAllocator, pArch->apHash);
    pArch->apHash = apNew;
    pArch->nSize = nNewSize;

    return SXRET_OK;
}

static sxi32 ArchiveHashInstallEntry(SyArchive* pArch, SyArchiveEntry* pEntry)
{
    if (pArch->nLoaded > pArch->nSize * 3)
    {
        ArchiveHashGrowTable(&(*pArch));
    }
    pEntry->nHash = pArch->xHash(SyStringData(&pEntry->sFileName), SyStringLength(&pEntry->sFileName));
    // Install the entry in its bucket.
    ArchiveHashBucketInstall(pArch->apHash, pEntry->nHash & (pArch->nSize - 1), pEntry);
    MACRO_LD_PUSH(pArch->pList, pEntry);
    pArch->nLoaded++;

    return SXRET_OK;
}

/**
 * Parse the End of central directory and report status
 */
static sxi32 ParseEndOfCentralDirectory(SyArchive* pArch, const unsigned char* zBuf)
{
    sxu32 nMagic = 0;

    /* Sanity check */
    sxi32 rc = SyLittleEndianUnpack32(&nMagic, zBuf, sizeof(sxu32));
    if ( /* rc != SXRET_OK || */nMagic != SXZIP_END_CENTRAL_MAGIC)
    {
        return SXERR_CORRUPT;
    }
    /* # of entries */
    rc = SyLittleEndianUnpack16((sxu16*)&pArch->nEntry, &zBuf[8], sizeof(sxu16));
    if ( /* rc != SXRET_OK || */ pArch->nEntry > SXI16_HIGH /* SXU16_HIGH */ )
    {
        return SXERR_CORRUPT;
    }
    /* Size of central directory */
    rc = SyLittleEndianUnpack32(&pArch->nCentralSize, &zBuf[12], sizeof(sxu32));
    if ( /*rc != SXRET_OK ||*/ pArch->nCentralSize > SXI32_HIGH)
    {
        return SXERR_CORRUPT;
    }
    /* Starting offset of central directory */
    rc = SyLittleEndianUnpack32(&pArch->nCentralOfft, &zBuf[16], sizeof(sxu32));
    if ( /*rc != SXRET_OK ||*/ pArch->nCentralSize > SXI32_HIGH)
    {
        return SXERR_CORRUPT;
    }

    return SXRET_OK;
}

/**
 * Fill the zip entry with the appropriate information from the central directory
 */
static sxi32 GetCentralDirectoryEntry(
    SyArchive* pArch,
    SyArchiveEntry* pEntry,
    const unsigned char* zCentral,
    sxu32* pNextOffset)
{
    SyString* pName = &pEntry->sFileName;
    sxu16 nDosDate, nDosTime;
    sxu16 nComment = 0;
    sxu32 nMagic = 0; /* cc -O6 warning */
    sxi32 rc;
    nDosDate = nDosTime = 0; /* cc -O6 warning */
    SXUNUSED(pArch);
    /* Sanity check */
    rc = SyLittleEndianUnpack32(&nMagic, zCentral, sizeof(sxu32));
    if ( /* rc != SXRET_OK || */ nMagic != SXZIP_CENTRAL_MAGIC)
    {
        rc = SXERR_CORRUPT;
        // Try to recover by examing the next central directory record.
        // Dont worry here,there is no risk of an infinite loop since the buffer size is delimited.

        /* pName->nByte = 0; nComment = 0; pName->nExtra = 0 */
        goto update;
    }
    // entry name length
    SyLittleEndianUnpack16((sxu16*)&pName->nByte, &zCentral[28], sizeof(sxu16));
    if (pName->nByte > SXI16_HIGH /* SXU16_HIGH */)
    {
        rc = SXERR_BIG;
        goto update;
    }
    /* Extra information */
    SyLittleEndianUnpack16(&pEntry->nExtra, &zCentral[30], sizeof(sxu16));
    /* Comment length  */
    SyLittleEndianUnpack16(&nComment, &zCentral[32], sizeof(sxu16));
    /* Compression method 0 == stored / 8 == deflated */
    rc = SyLittleEndianUnpack16(&pEntry->nComprMeth, &zCentral[10], sizeof(sxu16));
    /* DOS Timestamp */
    SyLittleEndianUnpack16(&nDosTime, &zCentral[12], sizeof(sxu16));
    SyLittleEndianUnpack16(&nDosDate, &zCentral[14], sizeof(sxu16));
    SyDosTimeFormat((nDosDate << 16u | nDosTime), &pEntry->sFmt);
    /* Little hack to fix month index  */
    pEntry->sFmt.tm_mon--;
    /* CRC32 */
    rc = SyLittleEndianUnpack32(&pEntry->nCrc, &zCentral[16], sizeof(sxu32));
    /* Content size before compression */
    rc = SyLittleEndianUnpack32(&pEntry->nByte, &zCentral[24], sizeof(sxu32));
    if (pEntry->nByte > SXI32_HIGH)
    {
        rc = SXERR_BIG;
        goto update;
    }
    //Content size after compression.
    // Note that if the file is stored pEntry->nByte should be equal to pEntry->nByteCompr
    rc = SyLittleEndianUnpack32(&pEntry->nByteCompr, &zCentral[20], sizeof(sxu32));
    if (pEntry->nByteCompr > SXI32_HIGH)
    {
        rc = SXERR_BIG;
        goto update;
    }
    // Finally grab the contents offset.
    SyLittleEndianUnpack32(&pEntry->nOfft, &zCentral[42], sizeof(sxu32));
    if (pEntry->nOfft > SXI32_HIGH)
    {
        rc = SXERR_BIG;
        goto update;
    }
    rc = SXRET_OK;
    update:
    // Update the offset to point to the next central directory record.
    *pNextOffset = SXZIP_CENTRAL_HDRSZ + pName->nByte + pEntry->nExtra + nComment;

    // Report failure or success
    return rc;
}

static sxi32 ZipFixOffset(SyArchiveEntry* pEntry, void* pSrc)
{
    unsigned char* zHdr = (unsigned char*)pSrc;
    zHdr = &zHdr[pEntry->nOfft];
    if (SyMemcmp(zHdr, "PK\003\004", sizeof(sxu32)) != 0)
    {
        return SXERR_CORRUPT;
    }
    sxu16 nNameLen;
    SyLittleEndianUnpack16(&nNameLen, &zHdr[26], sizeof(sxu16));
    sxu16 nExtra;
    SyLittleEndianUnpack16(&nExtra, &zHdr[28], sizeof(sxu16));
    /* Fix contents offset */
    pEntry->nOfft += SXZIP_LOCAL_HDRSZ + nExtra + nNameLen;

    return SXRET_OK;
}

/**
 * Extract all valid entries from the central directory
 */
static sxi32 ZipExtract(SyArchive* pArch, const unsigned char* zCentral, sxu32 nLen, void* pSrc)
{
    // Central Offset.
    sxu32 nIncr, nOfft = nIncr = 0;

    // End of central directory.
    const unsigned char* zEnd = &zCentral[nLen];

    for (;;)
    {
        if (&zCentral[nOfft] >= zEnd)
        {
            break;
        }
        /* Add a new entry */
        SyArchiveEntry* pEntry = (SyArchiveEntry*)SyMemBackendPoolAlloc(pArch->pAllocator, sizeof(SyArchiveEntry));
        if (pEntry == 0)
        {
            break;
        }
        SyZero(pEntry, sizeof(SyArchiveEntry));
        pEntry->nMagic = SXARCH_MAGIC;
        nIncr = 0;
        sxi32 rc = GetCentralDirectoryEntry(&(*pArch), pEntry, &zCentral[nOfft], &nIncr);
        if (rc == SXRET_OK)
        {
            // Fix the starting record offset so we can access entry contents correctly.
            rc = ZipFixOffset(pEntry, pSrc);
        }
        if (rc != SXRET_OK)
        {
            sxu32 nJmp = 0;
            SyMemBackendPoolFree(pArch->pAllocator, pEntry);
            // Try to recover by brute-forcing for a valid central directory record.
            sxi32 nBlobSearch = SyBlobSearch(
                (const void*)&zCentral[nOfft + nIncr],
                (sxu32)(zEnd - &zCentral[nOfft + nIncr]),
                (const void*)"PK\001\002",
                sizeof(sxu32),
                &nJmp);
            if (SXRET_OK == nBlobSearch)
            {
                // Check next entry.
                nOfft += nIncr + nJmp;
                continue;
            }
            // Giving up,archive is hopelessly corrupted.
            break;
        }
        // Entry name.
        SyString* pName = &pEntry->sFileName;
        pName->zString = (const char*)&zCentral[nOfft + SXZIP_CENTRAL_HDRSZ];
        if (pName->nByte <= 0 || (pEntry->nByte <= 0 && pName->zString[pName->nByte - 1] != '/'))
        {
            // Ignore zero length records (except folders) and records without names.
            SyMemBackendPoolFree(pArch->pAllocator, pEntry);
            // Check next entry.
            nOfft += nIncr;
            continue;
        }
        char* zName = SyMemBackendStrDup(pArch->pAllocator, pName->zString, pName->nByte);
        if (zName == 0)
        {
            SyMemBackendPoolFree(pArch->pAllocator, pEntry);
            // Check next entry.
            nOfft += nIncr;
            continue;
        }
        pName->zString = (const char*)zName;
        // Check for duplicates.
        SyArchiveEntry* pDup;
        rc = ArchiveHashGetEntry(&(*pArch), pName->zString, pName->nByte, &pDup);
        if (rc == SXRET_OK)
        {
            // Another entry with the same name exists ; link them together.
            pEntry->pNextName = pDup->pNextName;
            pDup->pNextName = pEntry;
            pDup->nDup++;
        }
        else
        {
            // Insert in hashtable
            ArchiveHashInstallEntry(pArch, pEntry);
        }
        // Check next record
        nOfft += nIncr;
    }
    pArch->pCursor = pArch->pList;

    return pArch->nLoaded > 0 ? SXRET_OK : SXERR_EMPTY;
}

PH7_PRIVATE sxi32 SyZipExtractFromBuf(SyArchive* pArch, const char* zBuf, sxu32 nLen)
{
#if defined(UNTRUST)
    if (SXARCH_INVALID(pArch) || zBuf == 0)
    {
        return SXERR_INVALID;
    }
#endif

    // The miminal size of a zip archive: LOCAL_HDR_SZ (30) + CENTRAL_HDR_SZ (46) + END_OF_CENTRAL_HDR_SZ (22)
    if (nLen < SXZIP_LOCAL_HDRSZ + SXZIP_CENTRAL_HDRSZ + SXZIP_END_CENTRAL_HDRSZ)
    {
        return SXERR_CORRUPT; /* Don't bother processing return immediately */
    }

    const unsigned char* zEnd = (unsigned char*)&zBuf[nLen - SXZIP_END_CENTRAL_HDRSZ];
    // Find the end of central directory
    while (((sxu32)((unsigned char*)&zBuf[nLen] - zEnd) < (SXZIP_END_CENTRAL_HDRSZ + SXI16_HIGH)) &&
           zEnd > (unsigned char*)zBuf &&
           SyMemcmp(zEnd, "PK\005\006", sizeof(sxu32)) != 0)
    {
        zEnd--;
    }
    // Parse the end of central directory
    sxi32 rc = ParseEndOfCentralDirectory(&(*pArch), zEnd);
    if (rc != SXRET_OK)
    {
        return rc;
    }

    // Find the starting offset of the central directory
    const unsigned char* zCentral = &zEnd[-(sxi32)pArch->nCentralSize];
    if (zCentral <= (unsigned char*)zBuf || SyMemcmp(zCentral, "PK\001\002", sizeof(sxu32)) != 0)
    {
        if (pArch->nCentralOfft >= nLen)
        {
            // Corrupted central directory offset */
            return SXERR_CORRUPT;
        }
        zCentral = (unsigned char*)&zBuf[pArch->nCentralOfft];
        if (
            SyMemcmp(zCentral, "PK\001\002", sizeof(sxu32)) != 0)
        {
            // Corrupted zip archive
            return SXERR_CORRUPT;
        }
        // Fall thru and extract all valid entries from the central directory
    }
    rc = ZipExtract(&(*pArch), zCentral, (sxu32)(zEnd - zCentral), (void*)zBuf);

    return rc;
}

/**
 * Default comparison function.
 */
static sxi32 ArchiveHashCmp(const SyString* pStr1, const SyString* pStr2)
{
    return SyStringCmp(pStr1, pStr2, SyMemcmp);
}

PH7_PRIVATE sxi32 SyArchiveInit(
    SyArchive* pArch,
    SyMemBackend* pAllocator,
    ProcHash xHash,
    ProcRawStrCmp xCmp)
{
#if defined(UNTRUST)
    if (pArch == 0)
    {
        return SXERR_EMPTY;
    }
#endif

    SyZero(pArch, sizeof(SyArchive));
    // Allocate a new hashtable.
    SyArchiveEntry** apHash = (SyArchiveEntry**)SyMemBackendAlloc(
        &(*pAllocator),
        SXARCHIVE_HASH_SIZE * sizeof(SyArchiveEntry*));
    if (apHash == 0)
    {
        return SXERR_MEM;
    }
    SyZero(apHash, SXARCHIVE_HASH_SIZE * sizeof(SyArchiveEntry*));
    pArch->apHash = apHash;
    pArch->xHash = xHash ? xHash : SyBinHash;
    pArch->xCmp = xCmp ? xCmp : ArchiveHashCmp;
    pArch->nSize = SXARCHIVE_HASH_SIZE;
    pArch->pAllocator = &(*pAllocator);
    pArch->nMagic = SXARCH_MAGIC;

    return SXRET_OK;
}

static sxi32 ArchiveReleaseEntry(SyMemBackend* pAllocator, SyArchiveEntry* pEntry)
{
    SyArchiveEntry* pDup = pEntry->pNextName;

    // Release duplicates first since there are not stored in the hashtable.
    for (;;)
    {
        if (pEntry->nDup == 0)
        {
            break;
        }
        SyArchiveEntry* pNextDup = pDup->pNextName;
        pDup->nMagic = 0x2661u;
        SyMemBackendFree(pAllocator, (void*)SyStringData(&pDup->sFileName));
        SyMemBackendPoolFree(pAllocator, pDup);
        pDup = pNextDup;
        pEntry->nDup--;
    }
    pEntry->nMagic = 0x2661u;
    SyMemBackendFree(pAllocator, (void*)SyStringData(&pEntry->sFileName));
    SyMemBackendPoolFree(pAllocator, pEntry);

    return SXRET_OK;
}

PH7_PRIVATE sxi32 SyArchiveRelease(SyArchive* pArch)
{
    SyArchiveEntry* pEntry, * pNext;
    pEntry = pArch->pList;
    for (;;)
    {
        if (pArch->nLoaded < 1)
        {
            break;
        }
        pNext = pEntry->pNext;
        MACRO_LD_REMOVE(pArch->pList, pEntry);
        ArchiveReleaseEntry(pArch->pAllocator, pEntry);
        pEntry = pNext;
        pArch->nLoaded--;
    }
    SyMemBackendFree(pArch->pAllocator, pArch->apHash);
    pArch->pCursor = 0;
    pArch->nMagic = 0x2626;

    return SXRET_OK;
}

PH7_PRIVATE sxi32 SyArchiveResetLoopCursor(SyArchive* pArch)
{
    pArch->pCursor = pArch->pList;

    return SXRET_OK;
}

PH7_PRIVATE sxi32 SyArchiveGetNextEntry(SyArchive* pArch, SyArchiveEntry** ppEntry)
{
    if (pArch->pCursor == 0)
    {
        // Rewind the cursor
        pArch->pCursor = pArch->pList;

        return SXERR_EOF;
    }
    *ppEntry = pArch->pCursor;
    SyArchiveEntry* pNext = pArch->pCursor->pNext;
    // Advance the cursor to the next entry
    pArch->pCursor = pNext;

    return SXRET_OK;
}

#endif // PH7_DISABLE_BUILTIN_FUNC

/*
 * Psuedo Random Number Generator (PRNG)
 * @authors: SQLite authors <http://www.sqlite.org/>
 * @status: Public Domain
 * NOTE:
 *  Nothing in this file or anywhere else in the library does any kind of
 *  encryption.The RC4 algorithm is being used as a PRNG (pseudo-random
 *  number generator) not as an encryption device.
 */
#define SXPRNG_MAGIC    0x13C4

#ifdef __UNIXES__
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <sys/time.h>
#endif

static sxi32 SyOSUtilRandomSeed(void* pBuf, sxu32 nLen, void* pUnused)
{
    char* zBuf = (char*)pBuf;
#ifdef __WINNT__
    // Yes,keep it uninitialized when compiling using the MinGW32 builds tools.
    DWORD nProcessID;
#elif defined(__UNIXES__)
    pid_t pid;
    int fd;
#else
    char zGarbage[128]; /* Yes,keep this buffer uninitialized */
#endif
    SXUNUSED(pUnused);
#ifdef __WINNT__
#ifndef __MINGW32__
    nProcessID = GetProcessId(GetCurrentProcess());
#endif
    SyMemcpy((const void*)&nProcessID, zBuf, SXMIN(nLen, sizeof(DWORD)));
    if ((sxu32)(&zBuf[nLen] - &zBuf[sizeof(DWORD)]) >= sizeof(SYSTEMTIME))
    {
        GetSystemTime((LPSYSTEMTIME)&zBuf[sizeof(DWORD)]);
    }
#elif defined(__UNIXES__)
    fd = open("/dev/urandom", O_RDONLY);
    if (fd >= 0)
    {
        if (read(fd, zBuf, nLen) > 0)
        {
            close(fd);
            return SXRET_OK;
        }
        /* FALL THRU */
    }
    close(fd);
    pid = getpid();
    SyMemcpy((const void*)&pid, zBuf, SXMIN(nLen, sizeof(pid_t)));
    if (&zBuf[nLen] - &zBuf[sizeof(pid_t)] >= (int)sizeof(struct timeval))
    {
        gettimeofday((struct timeval*)&zBuf[sizeof(pid_t)], 0);
    }
#else
    /* Fill with uninitialized data */
    SyMemcpy(zGarbage,zBuf,SXMIN(nLen,sizeof(zGarbage)));
#endif

    return SXRET_OK;
}

PH7_PRIVATE sxi32 SyRandomnessInit(
    SyPRNGCtx
    * pCtx,
    ProcRandomSeed xSeed,
    void* pUserData)
{
    if (pCtx->nMagic == SXPRNG_MAGIC)
    {
        // Already initialized.
        return SXRET_OK;
    }
    // Initialize the state of the random number generator once,
    // the first time this routine is called.The seed value does
    // not need to contain a lot of randomness since we are not
    // trying to do secure encryption or anything like that...
    char zSeed[256];
    if (xSeed == 0)
    {
        xSeed = SyOSUtilRandomSeed;
    }
    sxi32 rc = xSeed(zSeed, sizeof(zSeed), pUserData);
    if (rc != SXRET_OK)
    {
        return rc;
    }
    pCtx->i = pCtx->j = 0;
    for (sxu32 i = 0; i < SX_ARRAYSIZE(pCtx->s); i++)
    {
        pCtx->s[i] = (unsigned char)i;
    }
    for (sxu32 i = 0; i < sizeof(zSeed); i++)
    {
        pCtx->j += pCtx->s[i] + zSeed[i];
        sxu8 t = pCtx->s[pCtx->j];
        pCtx->s[pCtx->j] = pCtx->s[i];
        pCtx->s[i] = t;
    }
    pCtx->nMagic = SXPRNG_MAGIC;

    return SXRET_OK;
}

/**
 * Get a single 8-bit random value using the RC4 PRNG.
 */
static sxu8 randomByte(SyPRNGCtx* pCtx)
{
    /* Generate and return single random byte */
    pCtx->i++;
    sxu8 t = pCtx->s[pCtx->i];
    pCtx->j += t;
    pCtx->s[pCtx->i] = pCtx->s[pCtx->j];
    pCtx->s[pCtx->j] = t;
    t += pCtx->s[pCtx->i];

    return pCtx->s[t];
}

PH7_PRIVATE sxi32 SyRandomness(SyPRNGCtx* pCtx, void* pBuf, sxu32 nLen)
{
    unsigned char* zBuf = (unsigned char*)pBuf;
    unsigned char* zEnd = &zBuf[nLen];
#if defined(UNTRUST)
    if (pCtx == 0 || pBuf == 0 || nLen <= 0)
    {
        return SXERR_EMPTY;
    }
#endif
    if (pCtx->nMagic != SXPRNG_MAGIC)
    {
        return SXERR_CORRUPT;
    }
    for (;;)
    {
        if (zBuf >= zEnd)
        {
            break;
        }
        zBuf[0] = randomByte(pCtx);
        zBuf++;
        if (zBuf >= zEnd)
        {
            break;
        }
        zBuf[0] = randomByte(pCtx);
        zBuf++;
        if (zBuf >= zEnd)
        {
            break;
        }
        zBuf[0] = randomByte(pCtx);
        zBuf++;
        if (zBuf >= zEnd)
        {
            break;
        }
        zBuf[0] = randomByte(pCtx);
        zBuf++;
    }

    return SXRET_OK;
}

#ifndef PH7_DISABLE_BUILTIN_FUNC
#ifndef PH7_DISABLE_HASH_FUNC
/* SyRunTimeApi: sxhash.c */
/*
 * This code implements the MD5 message-digest algorithm.
 * The algorithm is due to Ron Rivest.This code was
 * written by Colin Plumb in 1993, no copyright is claimed.
 * This code is in the public domain; do with it what you wish.
 *
 * Equivalent code is available from RSA Data Security, Inc.
 * This code has been tested against that, and is equivalent,
 * except that you don't need to include two pages of legalese
 * with every copy.
 *
 * To compute the message digest of a chunk of bytes, declare an
 * MD5Context structure, pass it to MD5Init, call MD5Update as
 * needed on buffers full of bytes, and then call MD5Final, which
 * will fill a supplied 16-byte array with the digest.
 */
#define SX_MD5_BINSZ    16
#define SX_MD5_HEXSZ    32

/**
 * Note: this code is harmless on little-endian machines.
 */
static void byteReverse(unsigned char* buf, unsigned longs)
{
    do
    {
        sxu32 t = (sxu32)((unsigned)buf[3] << 8u | buf[2]) << 16u | ((unsigned)buf[1] << 8u | buf[0]);
        *(sxu32*)buf = t;
        buf += 4;
    } while (--longs);
}

/* The four core functions - F1 is optimized somewhat */
/* #define F1(x, y, z) (x & y | ~x & z) */
#ifdef F1
#undef F1
#endif
#ifdef F2
#undef F2
#endif
#ifdef F3
#undef F3
#endif
#ifdef F4
#undef F4
#endif

#define F1(x, y, z) (z ^ (x & (y ^ z)))
#define F2(x, y, z) F1(z, x, y)
#define F3(x, y, z) (x ^ y ^ z)
#define F4(x, y, z) (y ^ (x | ~z))

/* This is the central step in the MD5 algorithm.*/
#define SX_MD5STEP(f, w, x, y, z, data, s) \
        ( w += f(x, y, z) + data,  w = w<<s | w>>(32-s),  w += x )

/**
 * The core of the MD5 algorithm, this alters an existing MD5 hash to
 * reflect the addition of 16 longwords of new data.MD5Update blocks
 * the data and converts bytes into longwords for this routine.
 */
static void MD5Transform(sxu32 buf[4], const sxu32 in[16])
{
    register sxu32 a, b, c, d;

    a = buf[0];
    b = buf[1];
    c = buf[2];
    d = buf[3];

    SX_MD5STEP(F1, a, b, c, d, in[0] + 0xd76aa478, 7);
    SX_MD5STEP(F1, d, a, b, c, in[1] + 0xe8c7b756, 12);
    SX_MD5STEP(F1, c, d, a, b, in[2] + 0x242070db, 17);
    SX_MD5STEP(F1, b, c, d, a, in[3] + 0xc1bdceee, 22);
    SX_MD5STEP(F1, a, b, c, d, in[4] + 0xf57c0faf, 7);
    SX_MD5STEP(F1, d, a, b, c, in[5] + 0x4787c62a, 12);
    SX_MD5STEP(F1, c, d, a, b, in[6] + 0xa8304613, 17);
    SX_MD5STEP(F1, b, c, d, a, in[7] + 0xfd469501, 22);
    SX_MD5STEP(F1, a, b, c, d, in[8] + 0x698098d8, 7);
    SX_MD5STEP(F1, d, a, b, c, in[9] + 0x8b44f7af, 12);
    SX_MD5STEP(F1, c, d, a, b, in[10] + 0xffff5bb1, 17);
    SX_MD5STEP(F1, b, c, d, a, in[11] + 0x895cd7be, 22);
    SX_MD5STEP(F1, a, b, c, d, in[12] + 0x6b901122, 7);
    SX_MD5STEP(F1, d, a, b, c, in[13] + 0xfd987193, 12);
    SX_MD5STEP(F1, c, d, a, b, in[14] + 0xa679438e, 17);
    SX_MD5STEP(F1, b, c, d, a, in[15] + 0x49b40821, 22);

    SX_MD5STEP(F2, a, b, c, d, in[1] + 0xf61e2562, 5);
    SX_MD5STEP(F2, d, a, b, c, in[6] + 0xc040b340, 9);
    SX_MD5STEP(F2, c, d, a, b, in[11] + 0x265e5a51, 14);
    SX_MD5STEP(F2, b, c, d, a, in[0] + 0xe9b6c7aa, 20);
    SX_MD5STEP(F2, a, b, c, d, in[5] + 0xd62f105d, 5);
    SX_MD5STEP(F2, d, a, b, c, in[10] + 0x02441453, 9);
    SX_MD5STEP(F2, c, d, a, b, in[15] + 0xd8a1e681, 14);
    SX_MD5STEP(F2, b, c, d, a, in[4] + 0xe7d3fbc8, 20);
    SX_MD5STEP(F2, a, b, c, d, in[9] + 0x21e1cde6, 5);
    SX_MD5STEP(F2, d, a, b, c, in[14] + 0xc33707d6, 9);
    SX_MD5STEP(F2, c, d, a, b, in[3] + 0xf4d50d87, 14);
    SX_MD5STEP(F2, b, c, d, a, in[8] + 0x455a14ed, 20);
    SX_MD5STEP(F2, a, b, c, d, in[13] + 0xa9e3e905, 5);
    SX_MD5STEP(F2, d, a, b, c, in[2] + 0xfcefa3f8, 9);
    SX_MD5STEP(F2, c, d, a, b, in[7] + 0x676f02d9, 14);
    SX_MD5STEP(F2, b, c, d, a, in[12] + 0x8d2a4c8a, 20);

    SX_MD5STEP(F3, a, b, c, d, in[5] + 0xfffa3942, 4);
    SX_MD5STEP(F3, d, a, b, c, in[8] + 0x8771f681, 11);
    SX_MD5STEP(F3, c, d, a, b, in[11] + 0x6d9d6122, 16);
    SX_MD5STEP(F3, b, c, d, a, in[14] + 0xfde5380c, 23);
    SX_MD5STEP(F3, a, b, c, d, in[1] + 0xa4beea44, 4);
    SX_MD5STEP(F3, d, a, b, c, in[4] + 0x4bdecfa9, 11);
    SX_MD5STEP(F3, c, d, a, b, in[7] + 0xf6bb4b60, 16);
    SX_MD5STEP(F3, b, c, d, a, in[10] + 0xbebfbc70, 23);
    SX_MD5STEP(F3, a, b, c, d, in[13] + 0x289b7ec6, 4);
    SX_MD5STEP(F3, d, a, b, c, in[0] + 0xeaa127fa, 11);
    SX_MD5STEP(F3, c, d, a, b, in[3] + 0xd4ef3085, 16);
    SX_MD5STEP(F3, b, c, d, a, in[6] + 0x04881d05, 23);
    SX_MD5STEP(F3, a, b, c, d, in[9] + 0xd9d4d039, 4);
    SX_MD5STEP(F3, d, a, b, c, in[12] + 0xe6db99e5, 11);
    SX_MD5STEP(F3, c, d, a, b, in[15] + 0x1fa27cf8, 16);
    SX_MD5STEP(F3, b, c, d, a, in[2] + 0xc4ac5665, 23);

    SX_MD5STEP(F4, a, b, c, d, in[0] + 0xf4292244, 6);
    SX_MD5STEP(F4, d, a, b, c, in[7] + 0x432aff97, 10);
    SX_MD5STEP(F4, c, d, a, b, in[14] + 0xab9423a7, 15);
    SX_MD5STEP(F4, b, c, d, a, in[5] + 0xfc93a039, 21);
    SX_MD5STEP(F4, a, b, c, d, in[12] + 0x655b59c3, 6);
    SX_MD5STEP(F4, d, a, b, c, in[3] + 0x8f0ccc92, 10);
    SX_MD5STEP(F4, c, d, a, b, in[10] + 0xffeff47d, 15);
    SX_MD5STEP(F4, b, c, d, a, in[1] + 0x85845dd1, 21);
    SX_MD5STEP(F4, a, b, c, d, in[8] + 0x6fa87e4f, 6);
    SX_MD5STEP(F4, d, a, b, c, in[15] + 0xfe2ce6e0, 10);
    SX_MD5STEP(F4, c, d, a, b, in[6] + 0xa3014314, 15);
    SX_MD5STEP(F4, b, c, d, a, in[13] + 0x4e0811a1, 21);
    SX_MD5STEP(F4, a, b, c, d, in[4] + 0xf7537e82, 6);
    SX_MD5STEP(F4, d, a, b, c, in[11] + 0xbd3af235, 10);
    SX_MD5STEP(F4, c, d, a, b, in[2] + 0x2ad7d2bb, 15);
    SX_MD5STEP(F4, b, c, d, a, in[9] + 0xeb86d391, 21);

    buf[0] += a;
    buf[1] += b;
    buf[2] += c;
    buf[3] += d;
}

/**
 * Update context to reflect the concatenation of another buffer full of bytes.
 */
PH7_PRIVATE void MD5Update(MD5Context* ctx, const unsigned char* buf, unsigned int len)
{
    /* Update bitcount */
    sxu32 t = ctx->bits[0];
    if ((ctx->bits[0] = t + ((sxu32){len << 3u})) < t)
    {
        ctx->bits[1]++;
    } /* Carry from low to high */
    ctx->bits[1] += len >> 29u;
    // Bytes already in shsInfo->data
    t = (t >> 3u) & 0x3Fu;
    /* Handle any leading odd-sized chunks */
    if (t)
    {
        unsigned char* p = (unsigned char*)ctx->in + t;

        t = 64 - t;
        if (len < t)
        {
            SyMemcpy(buf, p, len);
            return;
        }
        SyMemcpy(buf, p, t);
        byteReverse(ctx->in, 16);
        MD5Transform(ctx->buf, (sxu32*)ctx->in);
        buf += t;
        len -= t;
    }
    /* Process data in 64-byte chunks */
    while (len >= 64)
    {
        SyMemcpy(buf, ctx->in, 64);
        byteReverse(ctx->in, 16);
        MD5Transform(ctx->buf, (sxu32*)ctx->in);
        buf += 64;
        len -= 64;
    }
    /* Handle any remaining bytes of data.*/
    SyMemcpy(buf, ctx->in, len);
}

/**
 * Final wrapup - pad to 64-byte boundary with the bit pattern 1 0* (64-bit count of bits processed, MSB-first)
 */
PH7_PRIVATE void MD5Final(unsigned char digest[16], MD5Context* ctx)
{
    /* Compute number of bytes mod 64 */
    unsigned count = (ctx->bits[0] >> 3u) & 0x3Fu;

    /* Set the first char of padding to 0x80.This is safe since there is always at least one byte free */
    unsigned char* p = ctx->in + count;
    *p++ = 0x80;

    /* Bytes of padding needed to make 64 bytes */
    count = 64 - 1 - count;

    /* Pad out to 56 mod 64 */
    if (count < 8)
    {
        /* Two lots of padding:  Pad the first block to 64 bytes */
        SyZero(p, count);
        byteReverse(ctx->in, 16);
        MD5Transform(ctx->buf, (sxu32*)ctx->in);

        /* Now fill the next block with 56 bytes */
        SyZero(ctx->in, 56);
    }
    else
    {
        /* Pad block to 56 bytes */
        SyZero(p, count - 8);
    }
    byteReverse(ctx->in, 14);

    /* Append length in bits and transform */
    ((sxu32*)ctx->in)[14] = ctx->bits[0];
    ((sxu32*)ctx->in)[15] = ctx->bits[1];

    MD5Transform(ctx->buf, (sxu32*)ctx->in);
    byteReverse((unsigned char*)ctx->buf, 4);
    SyMemcpy(ctx->buf, digest, 0x10);
    SyZero(ctx, sizeof(ctx)); // In case it's sensitive.
}

#undef F1
#undef F2
#undef F3
#undef F4

PH7_PRIVATE sxi32 MD5Init(MD5Context* pCtx)
{
    pCtx->buf[0] = 0x67452301;
    pCtx->buf[1] = 0xefcdab89;
    pCtx->buf[2] = 0x98badcfe;
    pCtx->buf[3] = 0x10325476;
    pCtx->bits[0] = 0;
    pCtx->bits[1] = 0;

    return SXRET_OK;
}

PH7_PRIVATE sxi32

SyMD5Compute(const void* pIn, sxu32 nLen, unsigned char zDigest[16])
{
    MD5Context sCtx;
    MD5Init(&sCtx);
    MD5Update(&sCtx, (const unsigned char*)pIn, nLen);
    MD5Final(zDigest, &sCtx);

    return SXRET_OK;
}
/*
 * SHA-1 in C
 * By Steve Reid <steve@edmweb.com>
 * Status: Public Domain
 */
/*
 * blk0() and blk() perform the initial expand.
 * I got the idea of expanding during the round function from SSLeay
 *
 * blk0le() for little-endian and blk0be() for big-endian.
 */
#if __GNUC__ && (defined(__i386__) || defined(__x86_64__))
/*
 * GCC by itself only generates left rotates.  Use right rotates if
 * possible to be kinder to dinky implementations with iterative rotate
 * instructions.
 */
#define SHA_ROT(op, x, k) \
        ({ unsigned int y; asm(op " %1,%0" : "=r" (y) : "I" (k), "0" (x)); y; })
#define rol(x, k) SHA_ROT("roll", x, k)
#define ror(x, k) SHA_ROT("rorl", x, k)

#else
/* Generic C equivalent */
#define SHA_ROT(x,l,r) ((x) << (l) | (x) >> (r))
#define rol(x,k) SHA_ROT(x,k,32-(k))
#define ror(x,k) SHA_ROT(x,32-(k),k)
#endif

#define blk0le(i) (block[i] = (ror(block[i],8)&0xFF00FF00) \
    |(rol(block[i],8)&0x00FF00FF))
#define blk0be(i) block[i]
#define blk(i) (block[i&15] = rol(block[(i+13)&15]^block[(i+8)&15] \
    ^block[(i+2)&15]^block[i&15],1))

/*
 * (R0+R1), R2, R3, R4 are the different operations (rounds) used in SHA1
 *
 * Rl0() for little-endian and Rb0() for big-endian.  Endianness is
 * determined at run-time.
 */
#define Rl0(v, w, x, y, z, i) \
    z+=((w&(x^y))^y)+blk0le(i)+0x5A827999+rol(v,5);w=ror(w,2);
#define Rb0(v, w, x, y, z, i) \
    z+=((w&(x^y))^y)+blk0be(i)+0x5A827999+rol(v,5);w=ror(w,2);
#define R1(v, w, x, y, z, i) \
    z+=((w&(x^y))^y)+blk(i)+0x5A827999+rol(v,5);w=ror(w,2);
#define R2(v, w, x, y, z, i) \
    z+=(w^x^y)+blk(i)+0x6ED9EBA1+rol(v,5);w=ror(w,2);
#define R3(v, w, x, y, z, i) \
    z+=(((w|x)&y)|(w&x))+blk(i)+0x8F1BBCDC+rol(v,5);w=ror(w,2);
#define R4(v, w, x, y, z, i) \
    z+=(w^x^y)+blk(i)+0xCA62C1D6+rol(v,5);w=ror(w,2);

/*
 * Hash a single 512-bit block. This is the core of the algorithm.
 */
#define a qq[0]
#define b qq[1]
#define c qq[2]
#define d qq[3]
#define e qq[4]

static void SHA1Transform(unsigned int state[5], const unsigned char buffer[64])
{
    unsigned int qq[5]; /* a, b, c, d, e; */
    static int one = 1;
    unsigned int block[16];
    SyMemcpy(buffer, (void*)block, 64);
    SyMemcpy(state, qq, 5 * sizeof(unsigned int));

    // Copy context->state[] to working vars:
    /*
     * a = state[0];
     * b = state[1];
     * c = state[2];
     * d = state[3];
     * e = state[4];
     */

    /* 4 rounds of 20 operations each. Loop unrolled. */
    if (1 == *(unsigned char*)&one)
    {
        Rl0(a, b, c, d, e, 0);
        Rl0(e, a, b, c, d, 1);
        Rl0(d, e, a, b, c, 2);
        Rl0(c, d, e, a, b, 3);
        Rl0(b, c, d, e, a, 4);
        Rl0(a, b, c, d, e, 5);
        Rl0(e, a, b, c, d, 6);
        Rl0(d, e, a, b, c, 7);
        Rl0(c, d, e, a, b, 8);
        Rl0(b, c, d, e, a, 9);
        Rl0(a, b, c, d, e, 10);
        Rl0(e, a, b, c, d, 11);
        Rl0(d, e, a, b, c, 12);
        Rl0(c, d, e, a, b, 13);
        Rl0(b, c, d, e, a, 14);
        Rl0(a, b, c, d, e, 15);
    }
    else
    {
        Rb0(a, b, c, d, e, 0);
        Rb0(e, a, b, c, d, 1);
        Rb0(d, e, a, b, c, 2);
        Rb0(c, d, e, a, b, 3);
        Rb0(b, c, d, e, a, 4);
        Rb0(a, b, c, d, e, 5);
        Rb0(e, a, b, c, d, 6);
        Rb0(d, e, a, b, c, 7);
        Rb0(c, d, e, a, b, 8);
        Rb0(b, c, d, e, a, 9);
        Rb0(a, b, c, d, e, 10);
        Rb0(e, a, b, c, d, 11);
        Rb0(d, e, a, b, c, 12);
        Rb0(c, d, e, a, b, 13);
        Rb0(b, c, d, e, a, 14);
        Rb0(a, b, c, d, e, 15);
    }
    R1(e, a, b, c, d, 16);
    R1(d, e, a, b, c, 17);
    R1(c, d, e, a, b, 18);
    R1(b, c, d, e, a, 19);
    R2(a, b, c, d, e, 20);
    R2(e, a, b, c, d, 21);
    R2(d, e, a, b, c, 22);
    R2(c, d, e, a, b, 23);
    R2(b, c, d, e, a, 24);
    R2(a, b, c, d, e, 25);
    R2(e, a, b, c, d, 26);
    R2(d, e, a, b, c, 27);
    R2(c, d, e, a, b, 28);
    R2(b, c, d, e, a, 29);
    R2(a, b, c, d, e, 30);
    R2(e, a, b, c, d, 31);
    R2(d, e, a, b, c, 32);
    R2(c, d, e, a, b, 33);
    R2(b, c, d, e, a, 34);
    R2(a, b, c, d, e, 35);
    R2(e, a, b, c, d, 36);
    R2(d, e, a, b, c, 37);
    R2(c, d, e, a, b, 38);
    R2(b, c, d, e, a, 39);
    R3(a, b, c, d, e, 40);
    R3(e, a, b, c, d, 41);
    R3(d, e, a, b, c, 42);
    R3(c, d, e, a, b, 43);
    R3(b, c, d, e, a, 44);
    R3(a, b, c, d, e, 45);
    R3(e, a, b, c, d, 46);
    R3(d, e, a, b, c, 47);
    R3(c, d, e, a, b, 48);
    R3(b, c, d, e, a, 49);
    R3(a, b, c, d, e, 50);
    R3(e, a, b, c, d, 51);
    R3(d, e, a, b, c, 52);
    R3(c, d, e, a, b, 53);
    R3(b, c, d, e, a, 54);
    R3(a, b, c, d, e, 55);
    R3(e, a, b, c, d, 56);
    R3(d, e, a, b, c, 57);
    R3(c, d, e, a, b, 58);
    R3(b, c, d, e, a, 59);
    R4(a, b, c, d, e, 60);
    R4(e, a, b, c, d, 61);
    R4(d, e, a, b, c, 62);
    R4(c, d, e, a, b, 63);
    R4(b, c, d, e, a, 64);
    R4(a, b, c, d, e, 65);
    R4(e, a, b, c, d, 66);
    R4(d, e, a, b, c, 67);
    R4(c, d, e, a, b, 68);
    R4(b, c, d, e, a, 69);
    R4(a, b, c, d, e, 70);
    R4(e, a, b, c, d, 71);
    R4(d, e, a, b, c, 72);
    R4(c, d, e, a, b, 73);
    R4(b, c, d, e, a, 74);
    R4(a, b, c, d, e, 75);
    R4(e, a, b, c, d, 76);
    R4(d, e, a, b, c, 77);
    R4(c, d, e, a, b, 78);
    R4(b, c, d, e, a, 79);

    /* Add the working vars back into context.state[] */
    state[0] += a;
    state[1] += b;
    state[2] += c;
    state[3] += d;
    state[4] += e;
}

#undef a
#undef b
#undef c
#undef d
#undef e

/**
 * SHA1Init - Initialize new context
 */
PH7_PRIVATE void SHA1Init(SHA1Context* context)
{
    /* SHA1 initialization constants */
    context->state[0] = 0x67452301;
    context->state[1] = 0xEFCDAB89;
    context->state[2] = 0x98BADCFE;
    context->state[3] = 0x10325476;
    context->state[4] = 0xC3D2E1F0;
    context->count[0] = context->count[1] = 0;
}

/**
 * Run your data through this.
 */
PH7_PRIVATE void SHA1Update(SHA1Context* context, const unsigned char* data, unsigned int len)
{
    unsigned int i = 0, j = context->count[0];
    if ((context->count[0] += len << 3u) < j)
    {
        context->count[1] += (len >> 29u) + 1;
    }
    j = (j >> 3u) & 63u;
    if ((j + len) > 63)
    {
        (void)SyMemcpy(data, &context->buffer[j], (i = 64 - j));
        SHA1Transform(context->state, context->buffer);
        for (; i + 63 < len; i += 64)
        {
            SHA1Transform(context->state, &data[i]);
        }
        j = 0;
    }
    else
    {
        i = 0;
    }
    (void)SyMemcpy(&data[i], &context->buffer[j], len - i);
}

/**
 * Add padding and return the message digest.
 */
PH7_PRIVATE void SHA1Final(SHA1Context* context, unsigned char digest[20])
{
    unsigned char finalCount[8];

    for (unsigned int i = 0; i < 8; i++)
    {
        // Endian independent
        finalCount[i] = (unsigned char)((context->count[(i >= 4 ? 0 : 1)] >> ((3 - (i & 3u)) * 8)) & 255u);
    }
    SHA1Update(context, (const unsigned char*)"\200", 1);
    while ((context->count[0] & 504u) != 448)
    {
        SHA1Update(context, (const unsigned char*)"\0", 1);
    }
    // Should cause a SHA1Transform()
    SHA1Update(context, finalCount, 8);

    if (digest)
    {
        for (unsigned int i = 0; i < 20; i++)
        {
            digest[i] = (unsigned char)((context->state[i >> 2u] >> ((3 - (i & 3u)) * 8)) & 255u);
        }
    }
}

#undef Rl0
#undef Rb0
#undef R1
#undef R2
#undef R3
#undef R4

PH7_PRIVATE sxi32

SySha1Compute(const void* pIn, sxu32 nLen, unsigned char zDigest[20])
{
    SHA1Context sCtx;
    SHA1Init(&sCtx);
    SHA1Update(&sCtx, (const unsigned char*)pIn, nLen);
    SHA1Final(&sCtx, zDigest);

    return SXRET_OK;
}

static const sxu32 crc32_table[] = {
    0x00000000, 0x77073096, 0xee0e612c, 0x990951ba,
    0x076dc419, 0x706af48f, 0xe963a535, 0x9e6495a3,
    0x0edb8832, 0x79dcb8a4, 0xe0d5e91e, 0x97d2d988,
    0x09b64c2b, 0x7eb17cbd, 0xe7b82d07, 0x90bf1d91,
    0x1db71064, 0x6ab020f2, 0xf3b97148, 0x84be41de,
    0x1adad47d, 0x6ddde4eb, 0xf4d4b551, 0x83d385c7,
    0x136c9856, 0x646ba8c0, 0xfd62f97a, 0x8a65c9ec,
    0x14015c4f, 0x63066cd9, 0xfa0f3d63, 0x8d080df5,
    0x3b6e20c8, 0x4c69105e, 0xd56041e4, 0xa2677172,
    0x3c03e4d1, 0x4b04d447, 0xd20d85fd, 0xa50ab56b,
    0x35b5a8fa, 0x42b2986c, 0xdbbbc9d6, 0xacbcf940,
    0x32d86ce3, 0x45df5c75, 0xdcd60dcf, 0xabd13d59,
    0x26d930ac, 0x51de003a, 0xc8d75180, 0xbfd06116,
    0x21b4f4b5, 0x56b3c423, 0xcfba9599, 0xb8bda50f,
    0x2802b89e, 0x5f058808, 0xc60cd9b2, 0xb10be924,
    0x2f6f7c87, 0x58684c11, 0xc1611dab, 0xb6662d3d,
    0x76dc4190, 0x01db7106, 0x98d220bc, 0xefd5102a,
    0x71b18589, 0x06b6b51f, 0x9fbfe4a5, 0xe8b8d433,
    0x7807c9a2, 0x0f00f934, 0x9609a88e, 0xe10e9818,
    0x7f6a0dbb, 0x086d3d2d, 0x91646c97, 0xe6635c01,
    0x6b6b51f4, 0x1c6c6162, 0x856530d8, 0xf262004e,
    0x6c0695ed, 0x1b01a57b, 0x8208f4c1, 0xf50fc457,
    0x65b0d9c6, 0x12b7e950, 0x8bbeb8ea, 0xfcb9887c,
    0x62dd1ddf, 0x15da2d49, 0x8cd37cf3, 0xfbd44c65,
    0x4db26158, 0x3ab551ce, 0xa3bc0074, 0xd4bb30e2,
    0x4adfa541, 0x3dd895d7, 0xa4d1c46d, 0xd3d6f4fb,
    0x4369e96a, 0x346ed9fc, 0xad678846, 0xda60b8d0,
    0x44042d73, 0x33031de5, 0xaa0a4c5f, 0xdd0d7cc9,
    0x5005713c, 0x270241aa, 0xbe0b1010, 0xc90c2086,
    0x5768b525, 0x206f85b3, 0xb966d409, 0xce61e49f,
    0x5edef90e, 0x29d9c998, 0xb0d09822, 0xc7d7a8b4,
    0x59b33d17, 0x2eb40d81, 0xb7bd5c3b, 0xc0ba6cad,
    0xedb88320, 0x9abfb3b6, 0x03b6e20c, 0x74b1d29a,
    0xead54739, 0x9dd277af, 0x04db2615, 0x73dc1683,
    0xe3630b12, 0x94643b84, 0x0d6d6a3e, 0x7a6a5aa8,
    0xe40ecf0b, 0x9309ff9d, 0x0a00ae27, 0x7d079eb1,
    0xf00f9344, 0x8708a3d2, 0x1e01f268, 0x6906c2fe,
    0xf762575d, 0x806567cb, 0x196c3671, 0x6e6b06e7,
    0xfed41b76, 0x89d32be0, 0x10da7a5a, 0x67dd4acc,
    0xf9b9df6f, 0x8ebeeff9, 0x17b7be43, 0x60b08ed5,
    0xd6d6a3e8, 0xa1d1937e, 0x38d8c2c4, 0x4fdff252,
    0xd1bb67f1, 0xa6bc5767, 0x3fb506dd, 0x48b2364b,
    0xd80d2bda, 0xaf0a1b4c, 0x36034af6, 0x41047a60,
    0xdf60efc3, 0xa867df55, 0x316e8eef, 0x4669be79,
    0xcb61b38c, 0xbc66831a, 0x256fd2a0, 0x5268e236,
    0xcc0c7795, 0xbb0b4703, 0x220216b9, 0x5505262f,
    0xc5ba3bbe, 0xb2bd0b28, 0x2bb45a92, 0x5cb36a04,
    0xc2d7ffa7, 0xb5d0cf31, 0x2cd99e8b, 0x5bdeae1d,
    0x9b64c2b0, 0xec63f226, 0x756aa39c, 0x026d930a,
    0x9c0906a9, 0xeb0e363f, 0x72076785, 0x05005713,
    0x95bf4a82, 0xe2b87a14, 0x7bb12bae, 0x0cb61b38,
    0x92d28e9b, 0xe5d5be0d, 0x7cdcefb7, 0x0bdbdf21,
    0x86d3d2d4, 0xf1d4e242, 0x68ddb3f8, 0x1fda836e,
    0x81be16cd, 0xf6b9265b, 0x6fb077e1, 0x18b74777,
    0x88085ae6, 0xff0f6a70, 0x66063bca, 0x11010b5c,
    0x8f659eff, 0xf862ae69, 0x616bffd3, 0x166ccf45,
    0xa00ae278, 0xd70dd2ee, 0x4e048354, 0x3903b3c2,
    0xa7672661, 0xd06016f7, 0x4969474d, 0x3e6e77db,
    0xaed16a4a, 0xd9d65adc, 0x40df0b66, 0x37d83bf0,
    0xa9bcae53, 0xdebb9ec5, 0x47b2cf7f, 0x30b5ffe9,
    0xbdbdf21c, 0xcabac28a, 0x53b39330, 0x24b4a3a6,
    0xbad03605, 0xcdd70693, 0x54de5729, 0x23d967bf,
    0xb3667a2e, 0xc4614ab8, 0x5d681b02, 0x2a6f2b94,
    0xb40bbe37, 0xc30c8ea1, 0x5a05df1b, 0x2d02ef8d,
};

#define CRC32C(c, d) (c = (crc32_table[(c ^ (d)) & 0xFFu] ^ (c >> 8u) ) )

static sxu32 SyCrc32Update(sxu32 crc32, const void* pSrc, sxu32 nLen)
{
    register unsigned char* zIn = (unsigned char*)pSrc;
    if (zIn == 0)
    {
        return crc32;
    }
    unsigned char* zEnd = &zIn[nLen];
    for (;;)
    {
        if (zIn >= zEnd)
        {
            break;
        }
        CRC32C(crc32, zIn[0]);
        zIn++;
        if (zIn >= zEnd)
        {
            break;
        }
        CRC32C(crc32, zIn[0]);
        zIn++;
        if (zIn >= zEnd)
        {
            break;
        }
        CRC32C(crc32, zIn[0]);
        zIn++;
        if (zIn >= zEnd)
        {
            break;
        }
        CRC32C(crc32, zIn[0]);
        zIn++;
    }

    return crc32;
}

PH7_PRIVATE sxu32 SyCrc32(const void* pSrc, sxu32 nLen)
{
    return SyCrc32Update(SXU32_HIGH, pSrc, nLen);
}

#endif // PH7_DISABLE_HASH_FUNC
#endif // PH7_DISABLE_BUILTIN_FUNC

#ifndef PH7_DISABLE_BUILTIN_FUNC

PH7_PRIVATE sxi32 SyBinToHexConsumer(
    const void* pIn,
    sxu32 nLen,
    ProcConsumer xConsumer,
    void* pConsumerData)
{
#if defined(UNTRUST)
    if (pIn == 0 || xConsumer == 0)
    {
        return SXERR_EMPTY;
    }
#endif

    const unsigned char* zIn = (const unsigned char*)pIn;
    const unsigned char* zEnd = &zIn[nLen];
    for (;;)
    {
        if (zIn >= zEnd)
        {
            break;
        }
        unsigned char zOut[3];
        static const unsigned char zHexTab[] = "0123456789abcdef";
        zOut[0] = zHexTab[zIn[0] >> 4u];
        zOut[1] = zHexTab[zIn[0] & 0x0Fu];
        sxi32 rc = xConsumer((const void*)zOut, sizeof(char) * 2, pConsumerData);
        if (rc != SXRET_OK)
        {
            return rc;
        }
        zIn++;
    }

    return SXRET_OK;
}

#endif // PH7_DISABLE_BUILTIN_FUNC


#include "API.h"

#include "ParserSession.h"
#include "Node.h"
#include "SymbolRegistration.h"

#if USE_EXPR_LIB
#include "ExprLibrary.h"
#include "WolframNumericArrayLibrary.h"
#endif // USE_EXPR_LIB

#include <cstdio> // for fopen


ParserSessionPtr CreateParserSession() {
    return new ParserSession();
}

void DestroyParserSession(ParserSessionPtr session) {
    delete session;
}

int ParserSessionInit(ParserSessionPtr session, Buffer buf, size_t bufLen, WolframLibraryData libData, SourceConvention srcConvention, uint32_t tabWidth, FirstLineBehavior firstLineBehavior, EncodingMode encodingMode) {
    
    BufferAndLength bufAndLen = BufferAndLength(buf, bufLen);
    
    return session->init(bufAndLen, libData, srcConvention, tabWidth, firstLineBehavior, encodingMode);
}

void ParserSessionDeinit(ParserSessionPtr session) {
    session->deinit();
}

NodeContainerPtr ParserSessionParseExpressions(ParserSessionPtr session) {
    return session->parseExpressions();
}

NodeContainerPtr ParserSessionTokenize(ParserSessionPtr session) {
    return session->tokenize();
}

NodeContainerPtr ParserSessionConcreteParseLeaf(ParserSessionPtr session, StringifyMode mode) {
    return session->concreteParseLeaf(mode);
}

NodeContainerPtr ParserSessionSafeString(ParserSessionPtr session) {
    return session->safeString();
}

void ParserSessionReleaseNodeContainer(ParserSessionPtr session, NodeContainerPtr C) {
    session->releaseNodeContainer(C);
}


void NodeContainerPrint(NodeContainerPtr C, std::ostream& s) {
    C->print(s);
}

int NodeContainerCheck(NodeContainerPtr C) {
    return C->check();
}

#if USE_EXPR_LIB
expr NodeContainerToExpr(ParserSessionPtr session, NodeContainerPtr C) {
    return C->toExpr(session);
}
#endif // USE_EXPR_LIB

#if USE_MATHLINK
void NodeContainerPut(ParserSessionPtr session, NodeContainerPtr C, MLINK callLink) {
    C->put(session, callLink);
}
#endif // USE_MATHLINK


mint WolframLibrary_getVersion() {
    return WolframLibraryVersion;
}

int WolframLibrary_initialize(WolframLibraryData libData) {
    return 0;
}

void WolframLibrary_uninitialize(WolframLibraryData libData) {
    
}


#if USE_EXPR_LIB
DLLEXPORT int CreateParserSession_LibraryLink(WolframLibraryData libData, mint Argc, MArgument *Args, MArgument Res) {
    
    if (Argc != 0) {
        return LIBRARY_FUNCTION_ERROR;
    }
    
    auto session = CreateParserSession();
    
    auto e = Expr_FromInteger64(reinterpret_cast<mint>(session));
    
    MArgument_setInteger(Res, reinterpret_cast<mint>(e));

    return LIBRARY_NO_ERROR;
}
#elif USE_MATHLINK
DLLEXPORT int CreateParserSession_LibraryLink(WolframLibraryData libData, MLINK callLink) {
    
    int mlLen;
    
    if (MLGetType(callLink) != MLTKFUNC) {
        return LIBRARY_FUNCTION_ERROR;
    }
    
    if (!MLTestHead(callLink, SYMBOL_LIST.Name, &mlLen)) {
        return LIBRARY_FUNCTION_ERROR;
    }
    
    auto len = static_cast<size_t>(mlLen);
    
    if (len != 0) {
        return LIBRARY_FUNCTION_ERROR;
    }
    
    auto session = CreateParserSession();

#if SIZEOF_VOID_P == 8
    if (!MLPutInteger64(callLink, reinterpret_cast<mlint64>(session))) {
        assert(false);
    }
#elif SIZEOF_VOID_P == 4
    if (!MLPutInteger32(callLink, reinterpret_cast<int>(session))) {
        assert(false);
    }
#endif // SIZEOF_VOID_P == 8
    
    return LIBRARY_NO_ERROR;
}
#endif // USE_EXPR_LIB


#if USE_EXPR_LIB
DLLEXPORT int DestroyParserSession_LibraryLink(WolframLibraryData libData, mint Argc, MArgument *Args, MArgument Res) {
    
    if (Argc != 1) {
        return LIBRARY_FUNCTION_ERROR;
    }
    
    auto mlSession = MArgument_getInteger(Args[0]);
    auto session = reinterpret_cast<ParserSessionPtr>(mlSession);
    
    DestroyParserSession(session);
    
    //
    // cannot use session after this
    //
    
    auto e = Expr_MEncodedStringToSymbolExpr(SYMBOL_NULL.Name);
    
    MArgument_setInteger(Res, reinterpret_cast<mint>(e));

    return LIBRARY_NO_ERROR;
}
#elif USE_MATHLINK
DLLEXPORT int DestroyParserSession_LibraryLink(WolframLibraryData libData, MLINK callLink) {
    
    int mlLen;
    
    if (MLGetType(callLink) != MLTKFUNC) {
        return LIBRARY_FUNCTION_ERROR;
    }
    
    if (!MLTestHead(callLink, SYMBOL_LIST.Name, &mlLen)) {
        return LIBRARY_FUNCTION_ERROR;
    }
    
    auto len = static_cast<size_t>(mlLen);
    
    if (len != 1) {
        return LIBRARY_FUNCTION_ERROR;
    }
    
    if (MLGetType(callLink) != MLTKINT) {
        return LIBRARY_FUNCTION_ERROR;
    }
    
#if SIZEOF_VOID_P == 8
    mlint64 mlSession;
    if (!MLGetInteger64(callLink, &mlSession)) {
        return LIBRARY_FUNCTION_ERROR;
    }
#elif SIZEOF_VOID_P == 4
    int mlSession;
    if (!MLGetInteger32(callLink, &mlSession)) {
        return LIBRARY_FUNCTION_ERROR;
    }
#endif // SIZEOF_VOID_P == 8
    
    auto session = reinterpret_cast<ParserSessionPtr>(mlSession);
    
    DestroyParserSession(session);
    
    //
    // cannot use session after this
    //
    
    if (!MLPutSymbol(callLink, SYMBOL_NULL.Name)) {
        assert(false);
    }
    
    return LIBRARY_NO_ERROR;
}
#endif // USE_EXPR_LIB


#if USE_EXPR_LIB
DLLEXPORT int ConcreteParseBytes_LibraryLink(WolframLibraryData libData, mint Argc, MArgument *Args, MArgument Res) {
    
    if (Argc != 5) {
        return LIBRARY_FUNCTION_ERROR;
    }
    
    auto mlSession = MArgument_getInteger(Args[0]);
    auto session = reinterpret_cast<ParserSessionPtr>(mlSession);
    
    auto arr = ScopedNumericArray(libData, Args[1]);

    auto mlSrcConvention = MArgument_getInteger(Args[2]);
    auto srcConvention = static_cast<SourceConvention>(mlSrcConvention);

    auto mlTabWidth = MArgument_getInteger(Args[3]);
    auto tabWidth = static_cast<uint32_t>(mlTabWidth);

    auto mlFirstLineBehavior = MArgument_getInteger(Args[4]);
    auto firstLineBehavior = static_cast<FirstLineBehavior>(mlFirstLineBehavior);

    auto numBytes = arr.size();

    auto data = arr.data();

    if (ParserSessionInit(session, data, numBytes, libData, srcConvention, tabWidth, firstLineBehavior, ENCODINGMODE_NORMAL)) {
        return LIBRARY_FUNCTION_ERROR;
    }

    auto C = ParserSessionParseExpressions(session);

    auto e = NodeContainerToExpr(session, C);

    ParserSessionReleaseNodeContainer(session, C);

    ParserSessionDeinit(session);
    
    MArgument_setInteger(Res, reinterpret_cast<mint>(e));

    return LIBRARY_NO_ERROR;
}
#elif USE_MATHLINK
DLLEXPORT int ConcreteParseBytes_LibraryLink(WolframLibraryData libData, MLINK callLink) {
    
    int mlLen;
    
    if (MLGetType(callLink) != MLTKFUNC) {
        return LIBRARY_FUNCTION_ERROR;
    }
    
    if (!MLTestHead(callLink, SYMBOL_LIST.Name, &mlLen)) {
        return LIBRARY_FUNCTION_ERROR;
    }
    
    auto len = static_cast<size_t>(mlLen);
    
    if (len != 5) {
        return LIBRARY_FUNCTION_ERROR;
    }
    
    if (MLGetType(callLink) != MLTKINT) {
        return LIBRARY_FUNCTION_ERROR;
    }
    
#if SIZEOF_VOID_P == 8
    mlint64 mlSession;
    if (!MLGetInteger64(callLink, &mlSession)) {
        assert(false);
    }
#elif SIZEOF_VOID_P == 4
    int mlSession;
    if (!MLGetInteger32(callLink, &mlSession)) {
        assert(false);
    }
#endif // SIZEOF_VOID_P == 8
    
    auto session = reinterpret_cast<ParserSessionPtr>(mlSession);
    
    if (MLGetType(callLink) != MLTKFUNC) {
        return LIBRARY_FUNCTION_ERROR;
    }
    
    if (!MLTestHead(callLink, SYMBOL_BYTEARRAY.Name, &mlLen)) {
        assert(false);
    }
    
    len = static_cast<size_t>(mlLen);
    
    if (len != 1) {
        return LIBRARY_FUNCTION_ERROR;
    }
    
    auto arr = ScopedMLByteArray(callLink);
    if (!arr.read()) {
        assert(false);
    }
    
    if (MLGetType(callLink) != MLTKINT) {
        return LIBRARY_FUNCTION_ERROR;
    }
    
    int mlSrcConvention;
    if (!MLGetInteger(callLink, &mlSrcConvention)) {
        assert(false);
    }
    
    auto srcConvention = static_cast<SourceConvention>(mlSrcConvention);
    
    if (MLGetType(callLink) != MLTKINT) {
        return LIBRARY_FUNCTION_ERROR;
    }
    
    int tabWidth;
    if (!MLGetInteger(callLink, &tabWidth)) {
        assert(false);
    }
    
    if (MLGetType(callLink) != MLTKINT) {
        return LIBRARY_FUNCTION_ERROR;
    }
    
    int mlFirstLineBehavior;
    if (!MLGetInteger(callLink, &mlFirstLineBehavior)) {
        assert(false);
    }
    
    auto firstLineBehavior = static_cast<FirstLineBehavior>(mlFirstLineBehavior);
    
    if (!MLNewPacket(callLink)) {
        assert(false);
    }
    
    if (ParserSessionInit(session, arr.get(), arr.getByteCount(), libData, srcConvention, tabWidth, firstLineBehavior, ENCODINGMODE_NORMAL)) {
        return LIBRARY_FUNCTION_ERROR;
    }
    
    auto C = ParserSessionParseExpressions(session);
    
    NodeContainerPut(session, C, callLink);
    
    ParserSessionReleaseNodeContainer(session, C);
    
    ParserSessionDeinit(session);
    
    return LIBRARY_NO_ERROR;
}
#endif // USE_EXPR_LIB


#if USE_EXPR_LIB
int TokenizeBytes_LibraryLink(WolframLibraryData libData, mint Argc, MArgument *Args, MArgument Res) {
    
    if (Argc != 5) {
        return LIBRARY_FUNCTION_ERROR;
    }
    
    auto mlSession = MArgument_getInteger(Args[0]);
    auto session = reinterpret_cast<ParserSessionPtr>(mlSession);
    
    auto arr = ScopedNumericArray(libData, Args[1]);

    auto mlSrcConvention = MArgument_getInteger(Args[2]);
    auto srcConvention = static_cast<SourceConvention>(mlSrcConvention);

    auto mlTabWidth = MArgument_getInteger(Args[3]);
    auto tabWidth = static_cast<int>(mlTabWidth);

    auto mlFirstLineBehavior = MArgument_getInteger(Args[4]);
    auto firstLineBehavior = static_cast<FirstLineBehavior>(mlFirstLineBehavior);

    auto numBytes = arr.size();

    auto data = arr.data();

    if (ParserSessionInit(session, data, numBytes, libData, srcConvention, tabWidth, firstLineBehavior, ENCODINGMODE_NORMAL)) {
        return LIBRARY_FUNCTION_ERROR;
    }

    auto C = ParserSessionTokenize(session);

    auto e = NodeContainerToExpr(session, C);

    ParserSessionReleaseNodeContainer(session, C);

    ParserSessionDeinit(session);
    
    MArgument_setInteger(Res, reinterpret_cast<mint>(e));

    return LIBRARY_NO_ERROR;
}
#elif USE_MATHLINK
int TokenizeBytes_LibraryLink(WolframLibraryData libData, MLINK callLink) {
    
    int mlLen;
    
    if (MLGetType(callLink) != MLTKFUNC) {
        return LIBRARY_FUNCTION_ERROR;
    }
    
    if (!MLTestHead(callLink, SYMBOL_LIST.Name, &mlLen)) {
        return LIBRARY_FUNCTION_ERROR;
    }
    
    auto len = static_cast<size_t>(mlLen);
    
    if (len != 5) {
        return LIBRARY_FUNCTION_ERROR;
    }
    
    if (MLGetType(callLink) != MLTKINT) {
        return LIBRARY_FUNCTION_ERROR;
    }
    
#if SIZEOF_VOID_P == 8
    mlint64 mlSession;
    if (!MLGetInteger64(callLink, &mlSession)) {
        return LIBRARY_FUNCTION_ERROR;
    }
#elif SIZEOF_VOID_P == 4
    int mlSession;
    if (!MLGetInteger32(callLink, &mlSession)) {
        return LIBRARY_FUNCTION_ERROR;
    }
#endif // SIZEOF_VOID_P == 8
    
    auto session = reinterpret_cast<ParserSessionPtr>(mlSession);
    
    if (MLGetType(callLink) != MLTKFUNC) {
        return LIBRARY_FUNCTION_ERROR;
    }
    
    if (!MLTestHead(callLink, SYMBOL_BYTEARRAY.Name, &mlLen)) {
        return LIBRARY_FUNCTION_ERROR;
    }
    
    len = static_cast<size_t>(mlLen);
    
    if (len != 1) {
        return LIBRARY_FUNCTION_ERROR;
    }
    
    auto arr = ScopedMLByteArray(callLink);
    if (!arr.read()) {
        assert(false);
    }
    
    if (MLGetType(callLink) != MLTKINT) {
        return LIBRARY_FUNCTION_ERROR;
    }
    
    int mlSrcConvention;
    if (!MLGetInteger(callLink, &mlSrcConvention)) {
        assert(false);
    }
    
    auto srcConvention = static_cast<SourceConvention>(mlSrcConvention);
    
    if (MLGetType(callLink) != MLTKINT) {
        return LIBRARY_FUNCTION_ERROR;
    }
    
    int tabWidth;
    if (!MLGetInteger(callLink, &tabWidth)) {
        return LIBRARY_FUNCTION_ERROR;
    }
    
    if (MLGetType(callLink) != MLTKINT) {
        return LIBRARY_FUNCTION_ERROR;
    }
    
    int mlFirstLineBehavior;
    if (!MLGetInteger(callLink, &mlFirstLineBehavior)) {
        return LIBRARY_FUNCTION_ERROR;
    }
    
    auto firstLineBehavior = static_cast<FirstLineBehavior>(mlFirstLineBehavior);
    
    if (!MLNewPacket(callLink) ) {
        assert(false);
    }
    
    if (ParserSessionInit(session, arr.get(), arr.getByteCount(), libData, srcConvention, tabWidth, firstLineBehavior, ENCODINGMODE_NORMAL)) {
        return LIBRARY_FUNCTION_ERROR;
    }
    
    auto C = ParserSessionTokenize(session);
    
    NodeContainerPut(session, C, callLink);
    
    ParserSessionReleaseNodeContainer(session, C);
    
    ParserSessionDeinit(session);
    
    return LIBRARY_NO_ERROR;
}
#endif // USE_EXPR_LIB


#if USE_EXPR_LIB
int ConcreteParseLeaf_LibraryLink(WolframLibraryData libData, mint Argc, MArgument *Args, MArgument Res) {
    
    if (Argc != 7) {
        return LIBRARY_FUNCTION_ERROR;
    }
    
    auto mlSession = MArgument_getInteger(Args[0]);
    auto session = reinterpret_cast<ParserSessionPtr>(mlSession);
    
    auto arr = ScopedNumericArray(libData, Args[1]);

    auto stringifyMode = MArgument_getInteger(Args[2]);

    auto mlSrcConvention = MArgument_getInteger(Args[3]);
    auto srcConvention = static_cast<SourceConvention>(mlSrcConvention);

    auto mlTabWidth = MArgument_getInteger(Args[4]);
    auto tabWidth = static_cast<int>(mlTabWidth);

    auto mlFirstLineBehavior = MArgument_getInteger(Args[5]);
    auto firstLineBehavior = static_cast<FirstLineBehavior>(mlFirstLineBehavior);

    auto mlEncodingMode = MArgument_getInteger(Args[6]);
    auto encodingMode = static_cast<EncodingMode>(mlEncodingMode);

    auto numBytes = arr.size();

    auto data = arr.data();

    if (ParserSessionInit(session, data, numBytes, libData, srcConvention, tabWidth, firstLineBehavior, encodingMode)) {
        return LIBRARY_FUNCTION_ERROR;
    }

    auto C = ParserSessionConcreteParseLeaf(session, static_cast<StringifyMode>(stringifyMode));

    auto e = NodeContainerToExpr(session, C);

    ParserSessionReleaseNodeContainer(session, C);

    ParserSessionDeinit(session);
    
    MArgument_setInteger(Res, reinterpret_cast<mint>(e));

    return LIBRARY_NO_ERROR;
}
#elif USE_MATHLINK
int ConcreteParseLeaf_LibraryLink(WolframLibraryData libData, MLINK callLink) {
    
    int mlLen;
    
    std::string unescaped;
    
    if (MLGetType(callLink) != MLTKFUNC) {
        return LIBRARY_FUNCTION_ERROR;
    }
    
    if (!MLTestHead(callLink, SYMBOL_LIST.Name, &mlLen))  {
        return LIBRARY_FUNCTION_ERROR;
    }
    
    auto len = static_cast<size_t>(mlLen);
    
    if (len != 7) {
        return LIBRARY_FUNCTION_ERROR;
    }
    
    if (MLGetType(callLink) != MLTKINT) {
        return LIBRARY_FUNCTION_ERROR;
    }
    
#if SIZEOF_VOID_P == 8
    mlint64 mlSession;
    if (!MLGetInteger64(callLink, &mlSession)) {
        return LIBRARY_FUNCTION_ERROR;
    }
#elif SIZEOF_VOID_P == 4
    int mlSession;
    if (!MLGetInteger32(callLink, &mlSession)) {
        return LIBRARY_FUNCTION_ERROR;
    }
#endif // SIZEOF_VOID_P == 8
    
    auto session = reinterpret_cast<ParserSessionPtr>(mlSession);
    
    if (MLGetType(callLink) != MLTKFUNC) {
        return LIBRARY_FUNCTION_ERROR;
    }
    
    if (!MLTestHead(callLink, SYMBOL_BYTEARRAY.Name, &mlLen)) {
        return LIBRARY_FUNCTION_ERROR;
    }
    
    len = static_cast<size_t>(mlLen);
    
    if (len != 1) {
        return LIBRARY_FUNCTION_ERROR;
    }
    
    auto arr = ScopedMLByteArray(callLink);
    if (!arr.read()) {
        assert(false);
    }
    
    if (MLGetType(callLink) != MLTKINT) {
        return LIBRARY_FUNCTION_ERROR;
    }
    
    int stringifyMode;
    if (!MLGetInteger(callLink, &stringifyMode)) {
        return LIBRARY_FUNCTION_ERROR;
    }
    
    if (MLGetType(callLink) != MLTKINT) {
        return LIBRARY_FUNCTION_ERROR;
    }
    
    int mlSrcConvention;
    if (!MLGetInteger(callLink, &mlSrcConvention)) {
        return LIBRARY_FUNCTION_ERROR;
    }
    
    auto srcConvention = static_cast<SourceConvention>(mlSrcConvention);
    
    if (MLGetType(callLink) != MLTKINT) {
        return LIBRARY_FUNCTION_ERROR;
    }
    
    int tabWidth;
    if (!MLGetInteger(callLink, &tabWidth)) {
        return LIBRARY_FUNCTION_ERROR;
    }
    
    if (MLGetType(callLink) != MLTKINT) {
        return LIBRARY_FUNCTION_ERROR;
    }
    
    int mlFirstLineBehavior;
    if (!MLGetInteger(callLink, &mlFirstLineBehavior)) {
        return LIBRARY_FUNCTION_ERROR;
    }
    
    auto firstLineBehavior = static_cast<FirstLineBehavior>(mlFirstLineBehavior);
    
    if (MLGetType(callLink) != MLTKINT) {
        return LIBRARY_FUNCTION_ERROR;
    }
    
    int mlEncodingMode;
    if (!MLGetInteger(callLink, &mlEncodingMode)) {
        return LIBRARY_FUNCTION_ERROR;
    }
    
    auto encodingMode = static_cast<EncodingMode>(mlEncodingMode);

    if (!MLNewPacket(callLink) ) {
        assert(false);
    }
    
    auto numBytes = arr.getByteCount();
    
    auto data = arr.get();
    
    if (ParserSessionInit(session, data, numBytes, libData, srcConvention, tabWidth, firstLineBehavior, encodingMode)) {
        return LIBRARY_FUNCTION_ERROR;
    }
    
    auto C = ParserSessionConcreteParseLeaf(session, static_cast<StringifyMode>(stringifyMode));
    
    NodeContainerPut(session, C, callLink);
    
    ParserSessionReleaseNodeContainer(session, C);
    
    ParserSessionDeinit(session);
    
    return LIBRARY_NO_ERROR;
}
#endif // USE_EXPR_LIB


#if USE_EXPR_LIB
int SafeString_LibraryLink(WolframLibraryData libData, mint Argc, MArgument *Args, MArgument Res) {
    
    if (Argc != 2) {
        return LIBRARY_FUNCTION_ERROR;
    }

    auto mlSession = MArgument_getInteger(Args[0]);
    auto session = reinterpret_cast<ParserSessionPtr>(mlSession);
    
    auto arr = ScopedNumericArray(libData, Args[1]);

    auto numBytes = arr.size();

    auto data = arr.data();

    if (ParserSessionInit(session, data, numBytes, libData, SOURCECONVENTION_LINECOLUMN, DEFAULT_TAB_WIDTH, FIRSTLINEBEHAVIOR_NOTSCRIPT, ENCODINGMODE_NORMAL)) {
        return LIBRARY_FUNCTION_ERROR;
    }

    auto C = ParserSessionSafeString(session);

    auto e = NodeContainerToExpr(session, C);

    ParserSessionReleaseNodeContainer(session, C);

    ParserSessionDeinit(session);
    
    MArgument_setInteger(Res, reinterpret_cast<mint>(e));

    return LIBRARY_NO_ERROR;
}
#elif USE_MATHLINK
int SafeString_LibraryLink(WolframLibraryData libData, MLINK callLink) {
    
    int mlLen;
    
    if (MLGetType(callLink) != MLTKFUNC) {
        return LIBRARY_FUNCTION_ERROR;
    }
    
    if (!MLTestHead(callLink, SYMBOL_LIST.Name, &mlLen)) {
        return LIBRARY_FUNCTION_ERROR;
    }
    
    auto len = static_cast<size_t>(mlLen);
    
    if (len != 2) {
        return LIBRARY_FUNCTION_ERROR;
    }
    
    if (MLGetType(callLink) != MLTKINT) {
        return LIBRARY_FUNCTION_ERROR;
    }
    
#if SIZEOF_VOID_P == 8
    mlint64 mlSession;
    if (!MLGetInteger64(callLink, &mlSession)) {
        return LIBRARY_FUNCTION_ERROR;
    }
#elif SIZEOF_VOID_P == 4
    int mlSession;
    if (!MLGetInteger32(callLink, &mlSession)) {
        return LIBRARY_FUNCTION_ERROR;
    }
#endif // SIZEOF_VOID_P == 8
    
    auto session = reinterpret_cast<ParserSessionPtr>(mlSession);
    
    if (MLGetType(callLink) != MLTKFUNC) {
        return LIBRARY_FUNCTION_ERROR;
    }
    
    if (!MLTestHead(callLink, SYMBOL_BYTEARRAY.Name, &mlLen)) {
        return LIBRARY_FUNCTION_ERROR;
    }
    
    len = static_cast<size_t>(mlLen);
    
    if (len != 1) {
        return LIBRARY_FUNCTION_ERROR;
    }
    
    auto arr = ScopedMLByteArray(callLink);
    if (!arr.read()) {
        assert(false);
    }
    
    if (!MLNewPacket(callLink) ) {
        assert(false);
    }
    
    if (ParserSessionInit(session, arr.get(), arr.getByteCount(), libData, SOURCECONVENTION_LINECOLUMN, DEFAULT_TAB_WIDTH, FIRSTLINEBEHAVIOR_NOTSCRIPT, ENCODINGMODE_NORMAL)) {
        return LIBRARY_FUNCTION_ERROR;
    }
    
    auto C = ParserSessionSafeString(session);
    
    NodeContainerPut(session, C, callLink);
    
    ParserSessionReleaseNodeContainer(session, C);
    
    ParserSessionDeinit(session);
    
    return LIBRARY_NO_ERROR;
}
#endif // USE_EXPR_LIB


ScopedFileBuffer::ScopedFileBuffer(Buffer inStrIn, size_t inLen) : buf(), len(), inited(false) {
    
    auto inStr = reinterpret_cast<const char *>(inStrIn);
    
    FILE *file = fopen(inStr, "rb");
    
    if (!file) {
        return;
    }
    
    if (fseek(file, 0, SEEK_END)) {
        return;
    }
    
    auto res = ftell(file);
    
    if (res < 0) {
        return;
    }
    len = res;
    
    rewind(file);
    
    buf = new unsigned char[len];
    
    inited = true;
    
    auto r = fread(buf, sizeof(unsigned char), len, file);
    
    if (r != len) {
        
        inited = false;
        
        delete[] buf;
    }
    
    fclose(file);
}

ScopedFileBuffer::~ScopedFileBuffer() {

    if (!inited) {
        return;
    }

    delete[] buf;
}

Buffer ScopedFileBuffer::getBuf() const {
    return buf;
}

size_t ScopedFileBuffer::getLen() const {
    return len;
}

bool ScopedFileBuffer::fail() const {
    return !inited;
}


#if USE_MATHLINK
ScopedMLString::ScopedMLString(MLINK link) : link(link), buf(NULL) {}

ScopedMLString::~ScopedMLString() {
    
    if (!buf) {
        return;
    }
    
    MLReleaseString(link, buf);
}

int ScopedMLString::read() {
    return MLGetString(link, &buf);
}

const char *ScopedMLString::get() const {
    return buf;
}
#endif // USE_MATHLINK


#if USE_MATHLINK
ScopedMLByteArray::ScopedMLByteArray(MLINK link) : link(link), buf(NULL), dims(), heads(), depth() {}

ScopedMLByteArray::~ScopedMLByteArray() {
    
    if (!buf) {
        return;
    }
    
    MLReleaseByteArray(link, buf, dims, heads, depth);
}

int ScopedMLByteArray::read() {
    return MLGetByteArray(link, &buf, &dims, &heads, &depth);
}

Buffer ScopedMLByteArray::get() const {
    return buf;
}

size_t ScopedMLByteArray::getByteCount() const {
    return *dims;
}
#endif // USE_MATHLINK


#if USE_EXPR_LIB
ScopedNumericArray::ScopedNumericArray(WolframLibraryData libData, MArgument Arg) : libData(libData), arr() {
    arr = MArgument_getMNumericArray(Arg);
}
    
ScopedNumericArray::~ScopedNumericArray() {
    libData->numericarrayLibraryFunctions->MNumericArray_disown(arr);
}

size_t ScopedNumericArray::size() const {
    return libData->numericarrayLibraryFunctions->MNumericArray_getFlattenedLength(arr);
}

Buffer ScopedNumericArray::data() const {
    return reinterpret_cast<Buffer>(libData->numericarrayLibraryFunctions->MNumericArray_getData(arr));
}
#endif // USE_EXPR_LIB

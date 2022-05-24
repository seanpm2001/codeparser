
#include "API.h"

#include "Parser.h" // for Parser
#include "ParseletRegistration.h" // for prefixParselets
#include "Parselet.h" // for Parselet impls
#include "Tokenizer.h" // for Tokenizer
#include "CharacterDecoder.h" // for CharacterDecoder
#include "ByteDecoder.h" // for ByteDecoder
#include "ByteBuffer.h" // for ByteBuffer
#include "ByteEncoder.h" // for ByteEncoder
#include "Utils.h" // for parseSourceConvention
#include "MyString.h"

#include <memory> // for unique_ptr
#ifdef WINDOWS_MATHLINK
#else
#include <signal.h> // for SIGINT
#endif // WINDOWS_MATHLINK
#include <vector>
#include <set>
#include <numeric> // for accumulate

bool validatePath(WolframLibraryData libData, const unsigned char *inStr, size_t len);

void NodeContainer::print(std::ostream& s) const {
    
    s << "List[";
    
    for (auto& NN : N) {
        NN->print(s);
        s << ", ";
    }
    
    s << "]";
}

bool NodeContainer::check() const {
    
    auto accum = std::accumulate(N.begin(), N.end(), true, [](bool a, const NodePtr& b){ return a && b->check(); });
    
    return accum;
}

void NodeContainerPrint(NodeContainer *C, std::ostream& stream) {
    C->print(stream);
}

int NodeContainerCheck(NodeContainer *C) {
    return C->check();
}


ParserSession::ParserSession() : fatalIssues(), nonFatalIssues(), bufAndLen(),
#if !NABORT
currentAbortQ(),
#endif // !NABORT
unsafeCharacterEncodingFlag() {
    
    TheByteBuffer = ByteBufferPtr(new ByteBuffer());
    TheByteDecoder = ByteDecoderPtr(new ByteDecoder());
    TheCharacterDecoder = CharacterDecoderPtr(new CharacterDecoder());
    TheTokenizer = TokenizerPtr(new Tokenizer());
    TheParser = ParserPtr(new Parser());
}

ParserSession::~ParserSession() {

    TheParser.reset(nullptr);
    TheTokenizer.reset(nullptr);
    TheCharacterDecoder.reset(nullptr);
    TheByteDecoder.reset(nullptr);
    TheByteBuffer.reset(nullptr);
}

void ParserSession::init(
    BufferAndLength bufAndLenIn,
    WolframLibraryData libData,
    SourceConvention srcConvention,
    uint32_t tabWidth,
    FirstLineBehavior firstLineBehavior,
    EncodingMode encodingMode) {
    
    fatalIssues.clear();
    nonFatalIssues.clear();
    
    bufAndLen = bufAndLenIn;
    
    unsafeCharacterEncodingFlag = UNSAFECHARACTERENCODING_OK;
    
    if (srcConvention == SOURCECONVENTION_UNKNOWN) {
        return;
    }
    
    TheByteBuffer->init(bufAndLen, libData);
    TheByteDecoder->init(srcConvention, tabWidth, encodingMode);
    TheCharacterDecoder->init(libData);
    TheTokenizer->init();
    TheParser->init(firstLineBehavior);
    
    if (libData) {
        
#if !NABORT
        currentAbortQ = [libData]() {
            //
            // AbortQ() returns a mint
            //
            bool res = libData->AbortQ();
            return res;
        };
#endif // !NABORT
    
    } else {
#if !NABORT
        currentAbortQ = nullptr;
#endif // !NABORT
    }
}

void ParserSession::deinit() {
    
    fatalIssues.clear();
    nonFatalIssues.clear();
    
    TheParser->deinit();
    TheTokenizer->deinit();
    TheCharacterDecoder->deinit();
    TheByteDecoder->deinit();
    TheByteBuffer->deinit();
}

NodeContainer *ParserSession::parseExpressions() {
    
    std::vector<NodePtr> nodes;
    
    //
    // Collect all expressions
    //
    {
        std::vector<NodePtr> exprs;
        
        ParserContext Ctxt;
        
        while (true) {
            
#if !NABORT
            if (TheParserSession->isAbort()) {
                
                break;
            }
#endif // !NABORT
            
            auto peek = TheParser->currentToken(Ctxt, TOPLEVEL);
            
            if (peek.Tok == TOKEN_ENDOFFILE) {
                break;
            }
            
            if (peek.Tok.isTrivia()) {
                
                exprs.push_back(LeafNodePtr(new LeafNode(std::move(peek))));
                
                TheParser->nextToken(peek);
                
                continue;
            }
            
            NodePtr Expr;
            //
            // special top-level handling of stray closers
            //
            if (peek.Tok.isCloser()) {
                
                Expr = contextSensitivePrefixToplevelCloserParselet->parse(peek, Ctxt);
                
            } else {
                Expr = prefixParselets[peek.Tok.value()]->parse(peek, Ctxt);
            }
            
            exprs.push_back(std::move(Expr));
            
        } // while (true)
        
        NodePtr Collected = NodePtr(new CollectedExpressionsNode(std::move(exprs)));
        
        nodes.push_back(std::move(Collected));
    }
    
    if (unsafeCharacterEncodingFlag != UNSAFECHARACTERENCODING_OK) {
        
        nodes.clear();
        
        std::vector<NodePtr> exprs;
        
        exprs.push_back(NodePtr(new MissingBecauseUnsafeCharacterEncodingNode(unsafeCharacterEncodingFlag)));
        
        NodePtr Collected = NodePtr(new CollectedExpressionsNode(std::move(exprs)));
        
        nodes.push_back(std::move(Collected));
    }
    
    //
    // Now handle the out-of-band expressions, i.e., issues and metadata
    //
    {
#if !NISSUES
        //
        // if there are fatal issues, then only send fatal issues
        //
        if (!fatalIssues.empty()) {
            
            nodes.push_back(NodePtr(new CollectedIssuesNode(std::move(fatalIssues))));
            
        } else {
            
            nodes.push_back(NodePtr(new CollectedIssuesNode(std::move(nonFatalIssues))));
        }
#else
        
        nodes.push_back(NodePtr(new CollectedIssuesNode()));
        
#endif // !NISSUES
    }
    
    {
        auto& SimpleLineContinuations = TheCharacterDecoder->getSimpleLineContinuations();

        nodes.push_back(NodePtr(new CollectedSourceLocationsNode(std::move(SimpleLineContinuations))));
    }
    
    {
        auto& ComplexLineContinuations = TheCharacterDecoder->getComplexLineContinuations();
        
        nodes.push_back(NodePtr(new CollectedSourceLocationsNode(std::move(ComplexLineContinuations))));
    }
    
    {
        auto& EmbeddedNewlines = TheTokenizer->getEmbeddedNewlines();

        nodes.push_back(NodePtr(new CollectedSourceLocationsNode(std::move(EmbeddedNewlines))));
    }
    
    {
        std::set<SourceLocation> tabs;
        
        auto& TokenizerEmbeddedTabs = TheTokenizer->getEmbeddedTabs();
        for (auto& T : TokenizerEmbeddedTabs) {
            tabs.insert(T);
        }
        
        auto& CharacterDecoderEmbeddedTabs = TheCharacterDecoder->getEmbeddedTabs();
        for (auto& T : CharacterDecoderEmbeddedTabs) {
            tabs.insert(T);
        }
        
        nodes.push_back(NodePtr(new CollectedSourceLocationsNode(std::move(tabs))));
    }
    
    auto C = new NodeContainer(std::move(nodes));
    
    return C;
}

NodeContainer *ParserSession::tokenize() {
    
    std::vector<NodePtr> nodes;
    
    while (true) {
        
        //
        // No need to check isAbort() inside tokenizer loops
        //
        
        auto Tok = TheTokenizer->currentToken(TOPLEVEL);
        
        if (Tok.Tok == TOKEN_ENDOFFILE) {
            break;
        }
        
        NodePtr N;
        if (Tok.Tok.isError()) {
            N = NodePtr(new ErrorNode(Tok));
        } else {
            N = NodePtr(new LeafNode(Tok));
        }
        
        nodes.push_back(std::move(N));
        
        TheTokenizer->nextToken(Tok);
        
    } // while (true)
    
    if (unsafeCharacterEncodingFlag != UNSAFECHARACTERENCODING_OK) {
        
        nodes.clear();
        
        auto N = NodePtr(new MissingBecauseUnsafeCharacterEncodingNode(unsafeCharacterEncodingFlag));

        nodes.push_back(std::move(N));
    }
    
    auto C = new NodeContainer(std::move(nodes));
    
    return C;
}

NodeContainer *ParserSession::listSourceCharacters() {
    
    std::vector<NodePtr> nodes;
    
    while (true) {
        
        //
        // No need to check isAbort() inside tokenizer loops
        //
        
        auto Char = TheByteDecoder->nextSourceCharacter0(TOPLEVEL);
        
        if (Char.isEndOfFile()) {
            break;
        }
        
        auto N = NodePtr(new SourceCharacterNode(Char));
        
        nodes.push_back(std::move(N));
        
    } // while (true)
    
    if (unsafeCharacterEncodingFlag != UNSAFECHARACTERENCODING_OK) {
        
        nodes.clear();
        
        auto N = NodePtr(new MissingBecauseUnsafeCharacterEncodingNode(unsafeCharacterEncodingFlag));

        nodes.push_back(std::move(N));
    }
    
    auto C = new NodeContainer(std::move(nodes));
    
    return C;
}


NodePtr ParserSession::concreteParseLeaf0(int mode) {
    
    switch (mode) {
        case STRINGIFYMODE_NORMAL: {
            auto Tok = TheTokenizer->nextToken0(TOPLEVEL);
            
            if (Tok.Tok.isError()) {
                return NodePtr(new ErrorNode(Tok));
            } else {
                return NodePtr(new LeafNode(Tok));
            }
        }
        case STRINGIFYMODE_TAG: {
            auto Tok = TheTokenizer->nextToken0_stringifyAsTag();
            
            if (Tok.Tok.isError()) {
                return NodePtr(new ErrorNode(Tok));
            } else {
                return NodePtr(new LeafNode(Tok));
            }
        }
        case STRINGIFYMODE_FILE: {
            auto Tok = TheTokenizer->nextToken0_stringifyAsFile();
            
            if (Tok.Tok.isError()) {
                return NodePtr(new ErrorNode(Tok));
            } else {
                return NodePtr(new LeafNode(Tok));
            }
        }
        default: {
            assert(false);
            return nullptr;
        }
    }
}

NodeContainer *ParserSession::concreteParseLeaf(StringifyMode mode) {
    
    std::vector<NodePtr> nodes;
    
    //
    // Collect all expressions
    //
    {
        std::vector<NodePtr> exprs;
        
        auto node = concreteParseLeaf0(mode);
        
        exprs.push_back(std::move(node));
        
        NodePtr Collected = NodePtr(new CollectedExpressionsNode(std::move(exprs)));
        
        nodes.push_back(std::move(Collected));
    }
    
    if (unsafeCharacterEncodingFlag != UNSAFECHARACTERENCODING_OK) {
        
        nodes.clear();
        
        std::vector<NodePtr> exprs;
        
        auto node = NodePtr(new MissingBecauseUnsafeCharacterEncodingNode(unsafeCharacterEncodingFlag));
        
        exprs.push_back(std::move(node));
        
        NodePtr Collected = NodePtr(new CollectedExpressionsNode(std::move(exprs)));
        
        nodes.push_back(std::move(Collected));
    }
    
    //
    // Collect all issues from the various components
    //
    {        
#if !NISSUES
        //
        // if there are fatal issues, then only send fatal issues
        //
        if (!fatalIssues.empty()) {
            
            nodes.push_back(NodePtr(new CollectedIssuesNode(std::move(fatalIssues))));
            
        } else {
            
            nodes.push_back(NodePtr(new CollectedIssuesNode(std::move(nonFatalIssues))));
        }
#else
        
        nodes.push_back(NodePtr(new CollectedIssuesNode()));
        
#endif // !NISSUES
    }
    
    {
        auto& SimpleLineContinuations = TheCharacterDecoder->getSimpleLineContinuations();
        
        nodes.push_back(NodePtr(new CollectedSourceLocationsNode(std::move(SimpleLineContinuations))));
    }
    
    {
        auto& ComplexLineContinuations = TheCharacterDecoder->getComplexLineContinuations();
        
        nodes.push_back(NodePtr(new CollectedSourceLocationsNode(std::move(ComplexLineContinuations))));
    }
    
    {
        auto& EmbeddedNewlines = TheTokenizer->getEmbeddedNewlines();
        
        nodes.push_back(NodePtr(new CollectedSourceLocationsNode(std::move(EmbeddedNewlines))));
    }
    
    {
        std::set<SourceLocation> tabs;
        
        auto& TokenizerEmbeddedTabs = TheTokenizer->getEmbeddedTabs();
        for (auto& T : TokenizerEmbeddedTabs) {
            tabs.insert(T);
        }
        
        auto& CharacterDecoderEmbeddedTabs = TheCharacterDecoder->getEmbeddedTabs();
        for (auto& T : CharacterDecoderEmbeddedTabs) {
            tabs.insert(T);
        }
        
        nodes.push_back(NodePtr(new CollectedSourceLocationsNode(std::move(tabs))));
    }
    
    auto C = new NodeContainer(std::move(nodes));
    
    return C;
}

NodeContainer *ParserSession::safeString() {
    
    std::vector<NodePtr> nodes;
    
    //
    // read all characters, just to set unsafeCharacterEncoding flag if necessary
    //
    while (true) {
        
        //
        // No need to check isAbort() inside tokenizer loops
        //
        
        auto Char = TheByteDecoder->nextSourceCharacter0(TOPLEVEL);
        
        if (Char.isEndOfFile()) {
            break;
        }
        
    } // while (true)
    
    auto N = NodePtr(new SafeStringNode(bufAndLen));
    
    nodes.push_back(std::move(N));
    
    if (unsafeCharacterEncodingFlag != UNSAFECHARACTERENCODING_OK) {
        
        nodes.clear();
        
        auto N = NodePtr(new MissingBecauseUnsafeCharacterEncodingNode(unsafeCharacterEncodingFlag));

        nodes.push_back(std::move(N));
    }
    
    auto C = new NodeContainer(std::move(nodes));
    
    return C;
}

//
// Does the file currently have permission to be read?
//
bool validatePath(WolframLibraryData libData, BufferAndLength bufAndLen) {
    
    if (!libData) {
        //
        // If running as a stand-alone executable, then always valid
        //
        return true;
    }
    
    auto inStr1 = reinterpret_cast<const char *>(bufAndLen.buffer);
    
    auto inStr2 = const_cast<char *>(inStr1);
    
    auto valid = libData->validatePath(inStr2, 'R');
    return valid;
}

void ParserSession::releaseContainer(NodeContainer *C) {
    delete C;
}

#if !NABORT
bool ParserSession::isAbort() const {
    if (!currentAbortQ) {
        return false;
    }
    
    return currentAbortQ();
}

NodePtr ParserSession::handleAbort() const {
    
    auto buf = TheByteBuffer->buffer;
    auto loc = TheByteDecoder->SrcLoc;
    
    auto A = Token(TOKEN_ERROR_ABORTED, BufferAndLength(buf), Source(loc));
    
    auto Aborted = NodePtr(new ErrorNode(A));
    
    return Aborted;
}
#endif // !NABORT

void ParserSession::setUnsafeCharacterEncodingFlag(UnsafeCharacterEncodingFlag flag) {
    unsafeCharacterEncodingFlag = flag;
}

void ParserSession::addIssue(IssuePtr I) {

    if (I->Sev == STRING_FATAL) {
        
        //
        // There may be situations where many (1000+) fatal errors are generated.
        // This has a noticeable impact on time to transfer for something that should be instantaneous.
        //
        // If there are, say, 10 fatal errors, then assume that the 11th is not going to give any new information,
        // and ignore.
        //
        if (fatalIssues.size() >= 10) {
            return;
        }

        fatalIssues.insert(std::move(I));

    } else {

        nonFatalIssues.insert(std::move(I));
    }
}

void ParserSessionCreate() {
    TheParserSession = ParserSessionPtr(new ParserSession());
}

void ParserSessionDestroy() {
    TheParserSession.reset(nullptr);
}

void ParserSessionInit(Buffer buf,
                      size_t bufLen,
                      WolframLibraryData libData,
                      SourceConvention srcConvention,
                      uint32_t tabWidth,
                      FirstLineBehavior firstLineBehavior,
                      EncodingMode encodingMode) {
    BufferAndLength bufAndLen = BufferAndLength(buf, bufLen);
    TheParserSession->init(bufAndLen, libData, srcConvention, tabWidth, firstLineBehavior, encodingMode);
}

void ParserSessionDeinit() {
    TheParserSession->deinit();
}

NodeContainer *ParserSessionParseExpressions() {
    return TheParserSession->parseExpressions();
}

NodeContainer *ParserSessionTokenize() {
    return TheParserSession->tokenize();
}

NodeContainer *ParserSessionListSourceCharacters() {
    return TheParserSession->listSourceCharacters();
}

NodeContainer *ParserSessionConcreteParseLeaf(StringifyMode mode) {
    return TheParserSession->concreteParseLeaf(mode);
}

void ParserSessionReleaseContainer(NodeContainer *C) {
    TheParserSession->releaseContainer(C);
}

ParserSessionPtr TheParserSession = nullptr;



void ByteBufferInit(Buffer buf,
                    size_t bufLen,
                    WolframLibraryData libData) {
    BufferAndLength bufAndLen = BufferAndLength(buf, bufLen);
    TheByteBuffer->init(bufAndLen, libData);
}

void ByteBufferDeinit() {
    TheByteBuffer->deinit();
}

void ByteDecoderInit(SourceConvention srcConvention, uint32_t TabWidth, EncodingMode encodingMode) {
    TheByteDecoder->init(srcConvention, TabWidth, encodingMode);
}

void ByteDecoderDeinit() {
    TheByteDecoder->deinit();
}



mint WolframLibrary_getVersion() {
    return WolframLibraryVersion;
}

int WolframLibrary_initialize(WolframLibraryData libData) {
    
    TheParserSession = ParserSessionPtr(new ParserSession);
    
    return 0;
}

void WolframLibrary_uninitialize(WolframLibraryData libData) {
    
    TheParserSession.reset(nullptr);
}


#if USE_MATHLINK

int ConcreteParseBytes_Listable_LibraryLink(WolframLibraryData libData, MLINK mlp) {
    
    int mlLen;
    
    if (!MLTestHead(mlp, SYMBOL_LIST->name(), &mlLen)) {
        return LIBRARY_FUNCTION_ERROR;
    }
    
    auto len = static_cast<size_t>(mlLen);
    
    if (len != 4) {
        return LIBRARY_FUNCTION_ERROR;
    }
    
    if (!MLTestHead(mlp, SYMBOL_LIST->name(), &mlLen)) {
        return LIBRARY_FUNCTION_ERROR;
    }
    
    len = static_cast<size_t>(mlLen);
    
    auto arrs = std::vector<ScopedMLByteArrayPtr>();
    arrs.reserve(len);
    
    for (size_t i = 0; i < len; i++) {
        
        auto arr = ScopedMLByteArrayPtr(new ScopedMLByteArray(mlp));
        if (!arr->read()) {
            return LIBRARY_FUNCTION_ERROR;
        }
        
        arrs.push_back(std::move(arr));
    }
    
    auto conventionStr = ScopedMLStringPtr(new ScopedMLString(mlp));
    if (!conventionStr->read()) {
        return LIBRARY_FUNCTION_ERROR;
    }
    auto srcConvention = Utils::parseSourceConvention(conventionStr->get());
    
    int tabWidth;
    if (!MLGetInteger(mlp, &tabWidth)) {
        return LIBRARY_FUNCTION_ERROR;
    }
    
    int mlFirstLineBehavior;
    if (!MLGetInteger(mlp, &mlFirstLineBehavior)) {
        return LIBRARY_FUNCTION_ERROR;
    }
    
    auto firstLineBehavior = static_cast<FirstLineBehavior>(mlFirstLineBehavior);
    
    if (!MLNewPacket(mlp) ) {
        return LIBRARY_FUNCTION_ERROR;
    }
    
    if (!MLPutFunction(mlp, SYMBOL_LIST->name(), mlLen)) {
        assert(false);
    }
    for (size_t i = 0; i < len; i++) {
        
        const auto& arr = arrs[i];
        
        auto bufAndLen = BufferAndLength(arr->get(), arr->getByteCount());
        
        TheParserSession->init(bufAndLen, libData, srcConvention, tabWidth, firstLineBehavior, ENCODINGMODE_NORMAL);
        
        auto C = TheParserSession->parseExpressions();
        
        C->put(mlp);
        
        TheParserSession->releaseContainer(C);
        
        TheParserSession->deinit();
    }
    
    return LIBRARY_NO_ERROR;
}

int TokenizeBytes_Listable_LibraryLink(WolframLibraryData libData, MLINK mlp) {
    
    int mlLen;
    
    if (!MLTestHead(mlp, SYMBOL_LIST->name(), &mlLen)) {
        return LIBRARY_FUNCTION_ERROR;
    }
    
    auto len = static_cast<size_t>(mlLen);
    
    if (len != 4) {
        return LIBRARY_FUNCTION_ERROR;
    }
    
    if (!MLTestHead(mlp, SYMBOL_LIST->name(), &mlLen)) {
        return LIBRARY_FUNCTION_ERROR;
    }

    len = static_cast<size_t>(mlLen);
    
    auto arrs = std::vector<ScopedMLByteArrayPtr>();
    arrs.reserve(len);
    
    for (size_t i = 0; i < len; i++) {
        
        auto arr = ScopedMLByteArrayPtr(new ScopedMLByteArray(mlp));
        if (!arr->read()) {
            return LIBRARY_FUNCTION_ERROR;
        }
        
        arrs.push_back(std::move(arr));
    }
    
    auto conventionStr = ScopedMLStringPtr(new ScopedMLString(mlp));
    if (!conventionStr->read()) {
        return LIBRARY_FUNCTION_ERROR;
    }
    auto srcConvention = Utils::parseSourceConvention(conventionStr->get());
    
    int tabWidth;
    if (!MLGetInteger(mlp, &tabWidth)) {
        return LIBRARY_FUNCTION_ERROR;
    }
    
    int mlFirstLineBehavior;
    if (!MLGetInteger(mlp, &mlFirstLineBehavior)) {
        return LIBRARY_FUNCTION_ERROR;
    }
    
    auto firstLineBehavior = static_cast<FirstLineBehavior>(mlFirstLineBehavior);
    
    if (!MLNewPacket(mlp) ) {
        return LIBRARY_FUNCTION_ERROR;
    }
    
    if (!MLPutFunction(mlp, SYMBOL_LIST->name(), mlLen)) {
        assert(false);
    }
    for (size_t i = 0; i < len; i++) {
        
        const auto& arr = arrs[i];
        
        auto bufAndLen = BufferAndLength(arr->get(), arr->getByteCount());
        
        TheParserSession->init(bufAndLen, libData, srcConvention, tabWidth, firstLineBehavior, ENCODINGMODE_NORMAL);
        
        auto C = TheParserSession->tokenize();
        
        C->put(mlp);
        
        TheParserSession->releaseContainer(C);
        
        TheParserSession->deinit();
    }
    
    return LIBRARY_NO_ERROR;
}

int ConcreteParseLeaf_LibraryLink(WolframLibraryData libData, MLINK mlp) {
    
    int mlLen;
    
    std::string unescaped;
    
    if (!MLTestHead(mlp, SYMBOL_LIST->name(), &mlLen))  {
        return LIBRARY_FUNCTION_ERROR;
    }
    
    auto len = static_cast<size_t>(mlLen);
    
    if (len != 6) {
        return LIBRARY_FUNCTION_ERROR;
    }
    
    auto inStr = ScopedMLUTF8StringPtr(new ScopedMLUTF8String(mlp));
    if (!inStr->read()) {
        return LIBRARY_FUNCTION_ERROR;
    }
    
    int stringifyMode;
    if (!MLGetInteger(mlp, &stringifyMode)) {
        return LIBRARY_FUNCTION_ERROR;
    }
    
    auto conventionStr = ScopedMLStringPtr(new ScopedMLString(mlp));
    if (!conventionStr->read()) {
        return LIBRARY_FUNCTION_ERROR;
    }
    auto srcConvention = Utils::parseSourceConvention(conventionStr->get());
    
    int tabWidth;
    if (!MLGetInteger(mlp, &tabWidth)) {
        return LIBRARY_FUNCTION_ERROR;
    }
    
    int mlFirstLineBehavior;
    if (!MLGetInteger(mlp, &mlFirstLineBehavior)) {
        return LIBRARY_FUNCTION_ERROR;
    }
    
    auto firstLineBehavior = static_cast<FirstLineBehavior>(mlFirstLineBehavior);
    
    int mlEncodingMode;
    if (!MLGetInteger(mlp, &mlEncodingMode)) {
        return LIBRARY_FUNCTION_ERROR;
    }
    
    auto encodingMode = static_cast<EncodingMode>(mlEncodingMode);

    if (!MLNewPacket(mlp) ) {
        return LIBRARY_FUNCTION_ERROR;
    }
    
    auto bufAndLen = BufferAndLength(inStr->get(), inStr->getByteCount());
    
    TheParserSession->init(bufAndLen, libData, srcConvention, tabWidth, firstLineBehavior, encodingMode);
    
    auto C = TheParserSession->concreteParseLeaf(static_cast<StringifyMode>(stringifyMode));
    
    C->put(mlp);
    
    TheParserSession->releaseContainer(C);
    
    TheParserSession->deinit();
    
    return LIBRARY_NO_ERROR;
}



int SafeString_LibraryLink(WolframLibraryData libData, MLINK mlp) {
    
    int mlLen;
    
    if (!MLTestHead(mlp, SYMBOL_LIST->name(), &mlLen)) {
        return LIBRARY_FUNCTION_ERROR;
    }
    
    auto len = static_cast<size_t>(mlLen);
    
    if (len != 1) {
        return LIBRARY_FUNCTION_ERROR;
    }
    
    auto arr = ScopedMLByteArrayPtr(new ScopedMLByteArray(mlp));
    if (!arr->read()) {
        return LIBRARY_FUNCTION_ERROR;
    }
    
    if (!MLNewPacket(mlp) ) {
        return LIBRARY_FUNCTION_ERROR;
    }
    
    auto bufAndLen = BufferAndLength(arr->get(), arr->getByteCount());
    
    TheParserSession->init(bufAndLen, libData, SOURCECONVENTION_LINECOLUMN, DEFAULT_TAB_WIDTH, FIRSTLINEBEHAVIOR_NOTSCRIPT, ENCODINGMODE_NORMAL);
    
    auto C = TheParserSession->safeString();
    
    C->put(mlp);
    
    TheParserSession->releaseContainer(C);
    
    TheParserSession->deinit();
    
    return LIBRARY_NO_ERROR;
}


ScopedMLUTF8String::ScopedMLUTF8String(MLINK mlp) : mlp(mlp), buf(NULL), b(), c() {}

ScopedMLUTF8String::~ScopedMLUTF8String() {
    
    if (buf == NULL) {
        return;
    }
    
    MLReleaseUTF8String(mlp, buf, b);
}

bool ScopedMLUTF8String::read() {
    return MLGetUTF8String(mlp, &buf, &b, &c);
}

Buffer ScopedMLUTF8String::get() const {
    return buf;
}

size_t ScopedMLUTF8String::getByteCount() const {
    return b;
}


ScopedMLString::ScopedMLString(MLINK mlp) : mlp(mlp), buf(NULL) {}

ScopedMLString::~ScopedMLString() {
    
    if (buf == NULL) {
        return;
    }
    
    MLReleaseString(mlp, buf);
}

bool ScopedMLString::read() {
    return MLGetString(mlp, &buf);
}

const char *ScopedMLString::get() const {
    return buf;
}


ScopedMLSymbol::ScopedMLSymbol(MLINK mlp) : mlp(mlp), sym(NULL) {}

ScopedMLSymbol::~ScopedMLSymbol() {
    if (sym != NULL) {
        MLReleaseSymbol(mlp, sym);
    }
}

bool ScopedMLSymbol::read() {
    return MLGetSymbol(mlp, &sym);
}

const char *ScopedMLSymbol::get() const {
    return sym;
}


ScopedMLFunction::ScopedMLFunction(MLINK mlp) : mlp(mlp), func(NULL), count() {}

ScopedMLFunction::~ScopedMLFunction() {
    if (func == NULL) {
        return;
    }
    
    MLReleaseSymbol(mlp, func);
}

bool ScopedMLFunction::read() {
    return MLGetFunction(mlp, &func, &count);
}

const char *ScopedMLFunction::getHead() const {
    return func;
}

int ScopedMLFunction::getArgCount() const {
    return count;
}


ScopedMLByteArray::ScopedMLByteArray(MLINK mlp) : mlp(mlp), buf(NULL), dims(), heads(), depth() {}

ScopedMLByteArray::~ScopedMLByteArray() {
    
    if (buf == NULL) {
        return;
    }
    
    MLReleaseByteArray(mlp, buf, dims, heads, depth);
}

bool ScopedMLByteArray::read() {
    return MLGetByteArray(mlp, &buf, &dims, &heads, &depth);
}

Buffer ScopedMLByteArray::get() const {
    return buf;
}

size_t ScopedMLByteArray::getByteCount() const {
    return dims[0];
}


ScopedMLEnvironmentParameter::ScopedMLEnvironmentParameter() : p(MLNewParameters(MLREVISION, MLAPIREVISION)) {}

ScopedMLEnvironmentParameter::~ScopedMLEnvironmentParameter() {
    MLReleaseParameters(p);
}

MLEnvironmentParameter ScopedMLEnvironmentParameter::get() {
    return p;
}

ScopedMLLoopbackLink::ScopedMLLoopbackLink() : mlp(NULL), ep(NULL) {
    
    ScopedMLEnvironmentParameterPtr p;
    int err;
    
#ifdef WINDOWS_MATHLINK
#else
    //
    // Needed because MathLink intercepts all signals
    //
    MLDoNotHandleSignalParameter(p.get(), SIGINT);
#endif // WINDOWS_MATHLINK
    
    ep = MLInitialize(p.get());
    if (ep == (MLENV)0) {
        
        return;
    }
    
    mlp = MLLoopbackOpen(ep, &err);
}

ScopedMLLoopbackLink::~ScopedMLLoopbackLink() {
    
    if (mlp == NULL) {
        return;
    }
    
    MLClose(mlp);
    
    MLDeinitialize(ep);
}

MLINK ScopedMLLoopbackLink::get() {
    return mlp;
}


void NodeContainer::put(MLINK mlp) const {
    
    if (!MLPutFunction(mlp, SYMBOL_LIST->name(), static_cast<int>(N.size()))) {
        assert(false);
    }
    
    for (auto& NN : N) {
        
#if !NABORT
        //
        // Check isAbort() inside loops
        //
        if (TheParserSession->isAbort()) {
            
            TheParserSession->handleAbort();
            return;
        }
#endif // !NABORT
        
        NN->put(mlp);
    }
}

#endif // USE_MATHLINK


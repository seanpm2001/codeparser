
#include "Parselet.h"
#include "ParseletRegistration.h" // for prefixParselets
#include "API.h" // for ParserSession
#include "Symbol.h"
#include "MyString.h"
#include "Parser.h"

//
// SemiSemiParselet is complicated enough to warrant its own implementation file.
// The syntax for ;; is complicated and has a lot of edge cases.
//

Precedence SemiSemiParselet::getPrecedence() const {
    return PRECEDENCE_SEMISEMI;
}

Token SemiSemiParselet::processImplicitTimes(Token TokIn) const {
    
    //
    // SemiSemi was already parsed with look-ahead with the assumption that implicit Times will be handled correctly
    //
    
    if (TheParser->getNodeStackSize() > 0) {
    
        auto& N = TheParser->topNode();
        
        if (auto B = dynamic_cast<BinaryNode *>(N.get())) {
            
            auto Op = B->getOp();
            
            if (Op == SYMBOL_SPAN) {
            
#if !NISSUES
            {
                auto I = IssuePtr(new SyntaxIssue(STRING_UNEXPECTEDIMPLICITTIMES, "Unexpected implicit ``Times`` between ``Spans``.", STRING_WARNING, TokIn.Src, 0.75, {}, {}));

                TheParserSession->addIssue(std::move(I));
            }
#endif // !NISSUES
                
                return Token(TOKEN_FAKE_IMPLICITTIMES, TokIn.BufLen.buffer, TokIn.Src.Start);
            }
            
            //
            // there is a Node, but it is not a Span
            //
            
            return TokIn;
        }
        
        if (auto T = dynamic_cast<TernaryNode *>(N.get())) {
            
            auto Op = T->getOp();
            
            if (Op == SYMBOL_SPAN) {
                
#if !NISSUES
            {
                auto I = IssuePtr(new SyntaxIssue(STRING_UNEXPECTEDIMPLICITTIMES, "Unexpected implicit ``Times`` between ``Spans``.", STRING_WARNING, TokIn.Src, 0.75, {}, {}));

                TheParserSession->addIssue(std::move(I));
            }
#endif // !NISSUES
                
                return Token(TOKEN_FAKE_IMPLICITTIMES, TokIn.BufLen.buffer, TokIn.Src.Start);
            }
            
            //
            // there is a Node, but it is not a Span
            //
            
            return TokIn;
        }
        
        //
        // there is a Node, but it is not a Span
        //
        
        return TokIn;
    }
    
    //
    // no Node, so this means that Args has already started
    //
    
#if !NISSUES
    {
        auto I = IssuePtr(new SyntaxIssue(STRING_UNEXPECTEDIMPLICITTIMES, "Unexpected implicit ``Times`` between ``Spans``.", STRING_WARNING, TokIn.Src, 0.75, {}, {}));

        TheParserSession->addIssue(std::move(I));
    }
#endif // !NISSUES
            
    return Token(TOKEN_FAKE_IMPLICITTIMES, TokIn.BufLen.buffer, TokIn.Src.Start);
}

ParseFunction SemiSemiParselet::parsePrefix() const {
    return SemiSemiParselet_parsePrefix;
}

void SemiSemiParselet_parsePrefix(ParseletPtr P, Token TokIn) {
    
    TheParser->pushContextV(PRECEDENCE_SEMISEMI);
    
    TheParser->appendArg(NodePtr(new LeafNode(Token(TOKEN_FAKE_IMPLICITONE, TokIn.BufLen.buffer, TokIn.Src.Start))));
    
    MUSTTAIL
    return SemiSemiParselet_parseInfix(P, TokIn);
}

ParseFunction SemiSemiParselet::parseInfix() const {
    return SemiSemiParselet_parseInfix;
}

void SemiSemiParselet_parseInfix(ParseletPtr P, Token TokIn) {
    
    TheParser->appendArg(NodePtr(new LeafNode(TokIn)));
    
    TheParser->nextToken(TokIn);
    
    MUSTTAIL
    return SemiSemiParselet_parse1(P, TokIn/*ignored*/);
}

void SemiSemiParselet_parse1(ParseletPtr P, Token Ignored) {
    
    auto& Trivia1 = TheParser->getTrivia1();
    
    auto SecondTok = TheParser->currentToken(TOPLEVEL);
    //
    // Span should not cross toplevel newlines
    //
    SecondTok = TheParser->eatTriviaButNotToplevelNewlines(SecondTok, TOPLEVEL, Trivia1);
    
    TheParser->appendArgs(Trivia1);
    
    //
    // a;;
    //  ^~TokIn
    //
    
    if (!SecondTok.Tok.isPossibleBeginning()) {
        
        //
        // a;;&
        //    ^SecondTok
        //
        
        auto Implicit = Token(TOKEN_FAKE_IMPLICITALL, SecondTok.BufLen.buffer, SecondTok.Src.Start);
        
        TheParser->pushNode(NodePtr(new LeafNode(Implicit)));
        
        MUSTTAIL
        return SemiSemiParselet_reduceBinary(P, Ignored);
    }
    
    if (SecondTok.Tok != TOKEN_SEMISEMI) {
        
        //
        // a;;b
        //    ^SecondTok
        //
        
        auto& Ctxt = TheParser->topContext();
        assert(Ctxt.F == nullptr);
        assert(Ctxt.P == nullptr);
        Ctxt.F = SemiSemiParselet_parse2;
        Ctxt.P = P;
        
        auto P2 = prefixParselets[SecondTok.Tok.value()];
        
        MUSTTAIL
        return (P2->parsePrefix())(P2, SecondTok);
    }
    
    //
    // a;;;;
    //    ^~SecondTok
    //
    
    TheParser->pushNode(NodePtr(new LeafNode(Token(TOKEN_FAKE_IMPLICITALL, SecondTok.BufLen.buffer, SecondTok.Src.Start))));
    
    TheParser->nextToken(SecondTok);
    
    auto ThirdTok = TheParser->currentToken(TOPLEVEL);
    //
    // Span should not cross toplevel newlines
    //
    ThirdTok = TheParser->eatTriviaButNotToplevelNewlines(ThirdTok, TOPLEVEL, Trivia1);
    
    if (!ThirdTok.Tok.isPossibleBeginning() || ThirdTok.Tok == TOKEN_SEMISEMI) {
        
        //
        // a;;;;&
        //      ^ThirdTok
        //
        
        //
        // a;;;;;;
        //      ^~ThirdTok
        //
        
        Trivia1.reset();
        SecondTok.reset();
        
        MUSTTAIL
        return SemiSemiParselet_reduceBinary(P, Ignored);
    }
    
    //
    // a;;;;b
    //      ^ThirdTok
    //
    
    TheParser->shift();
    
    TheParser->appendArg(NodePtr(new LeafNode(SecondTok)));
    
    TheParser->appendArgs(Trivia1);
    
    auto& Ctxt = TheParser->topContext();
    assert(Ctxt.F == nullptr);
    assert(Ctxt.P == nullptr);
    Ctxt.F = SemiSemiParselet_reduceTernary;
    Ctxt.P = P;
    
    auto P2 = prefixParselets[ThirdTok.Tok.value()];
    
    MUSTTAIL
    return (P2->parsePrefix())(P2, ThirdTok);
}

void SemiSemiParselet_parse2(ParseletPtr P, Token Ignored) {
    
    auto& Trivia1 = TheParser->getTrivia1();
    
    auto ThirdTok = TheParser->currentToken(TOPLEVEL);
    //
    // Span should not cross toplevel newlines
    //
    ThirdTok = TheParser->eatTriviaButNotToplevelNewlines(ThirdTok, TOPLEVEL, Trivia1);
    
    if (!ThirdTok.Tok.isPossibleBeginning() || ThirdTok.Tok != TOKEN_SEMISEMI) {
        
        //
        // a;;b&
        //     ^ThirdTok
        //
        
        //
        // \[Integral];;x\[DifferentialD]x
        //               ^~~~~~~~~~~~~~~~ThirdTok
        //
        
        Trivia1.reset();
        
        MUSTTAIL
        return SemiSemiParselet_reduceBinary(P, Ignored);
    }
    
    //
    // a;;b;;
    //     ^~ThirdTok
    //
    
    TheParser->nextToken(ThirdTok);
    
    auto& Trivia2 = TheParser->getTrivia2();
    
    auto FourthTok = TheParser->currentToken(TOPLEVEL);
    //
    // Span should not cross toplevel newlines
    //
    FourthTok = TheParser->eatTriviaButNotToplevelNewlines(FourthTok, TOPLEVEL, Trivia2);
    
    if (!FourthTok.Tok.isPossibleBeginning() || FourthTok.Tok == TOKEN_SEMISEMI) {
        
        //
        // a;;b;;&
        //       ^FourthTok
        //
        
        //
        // a;;b;;;;
        //       ^~FourthTok
        //
        
        Trivia2.reset();
        ThirdTok.reset();
        Trivia1.reset();
        
        MUSTTAIL
        return SemiSemiParselet_reduceBinary(P, Ignored);
    }
        
    //
    // a;;b;;c
    //       ^FourthTok
    //
    
    TheParser->shift();
    
    TheParser->appendArgs(Trivia2);
    TheParser->appendArg(NodePtr(new LeafNode(ThirdTok)));
    TheParser->appendArgs(Trivia2);
    
    auto& Ctxt = TheParser->topContext();
    assert(Ctxt.F == SemiSemiParselet_parse2);
    assert(Ctxt.P == P);
    Ctxt.F = SemiSemiParselet_reduceTernary;
    Ctxt.P = P;
    
    auto P2 = prefixParselets[FourthTok.Tok.value()];
    
    MUSTTAIL
    return (P2->parsePrefix())(P2, FourthTok);
}

void SemiSemiParselet_reduceBinary(ParseletPtr P, Token Ignored) {
    
    TheParser->shift();
    
    TheParser->pushNode(NodePtr(new BinaryNode(SYMBOL_SPAN, TheParser->popContext())));
    
    MUSTTAIL
    return Parser_parseClimb(nullptr, Ignored);
}

void SemiSemiParselet_reduceTernary(ParseletPtr P, Token Ignored) {
    
    TheParser->shift();
    
    TheParser->pushNode(NodePtr(new TernaryNode(SYMBOL_SPAN, TheParser->popContext())));
    
    MUSTTAIL
    return Parser_parseClimb(nullptr, Ignored);
}

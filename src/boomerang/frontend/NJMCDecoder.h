#pragma region License
/*
 * This file is part of the Boomerang Decompiler.
 *
 * See the file "LICENSE.TERMS" for information on usage and
 * redistribution of this file, and for a DISCLAIMER OF ALL
 * WARRANTIES.
 */
#pragma endregion License
#pragma once


#include "boomerang/db/RTL.h"
#include "boomerang/db/ssl/RTLInstDict.h"
#include "boomerang/db/statements/BranchStatement.h"
#include "boomerang/db/statements/BoolAssign.h"

#include "boomerang/util/Types.h"
#include "boomerang/frontend/Decoder.h"


class IBinaryImage;

/**
 * The NJMCDecoder class is a class that contains NJMC generated decoding methods.
 */
class NJMCDecoder : public IDecoder
{
public:
    /**
     * \param       prog Pointer to the Prog object
     */
    NJMCDecoder(Prog *prog);

    /// \copydoc IDecoder::~IDecoder
    virtual ~NJMCDecoder() = default;

    RTLInstDict& getRTLDict() { return m_rtlDict; }

    /**
     * \brief   Process an indirect jump instruction
     * \param   name name of instruction (for debugging)
     * \param   size size of instruction in bytes
     * \param   dest destination Exp*
     * \param   pc native pc
     * \param   stmts list of statements (?)
     * \param   result ref to decoder result object
     */
    void processComputedJump(const char *name, int size, SharedExp dest, Address pc,
                             std::list<Statement *> *stmts, DecodeResult& result);

    /**
     * \brief   Process an indirect call instruction
     * \param   name name of instruction (for debugging)
     * \param   size size of instruction in bytes
     * \param   dest destination Exp*
     * \param   pc native pc
     * \param   stmts list of statements (?)
     * \param   result ref to decoder result object
     */
    void processComputedCall(const char *name, int size, SharedExp dest, Address pc,
                             std::list<Statement *> *stmts, DecodeResult& result);

    /// \copydoc IInstructionTranslator::getRegName
    QString getRegName(int idx) const override;

    /// \copydoc IInstructionTranslator::getRegSize
    int getRegSize(int idx) const override;

    /// \copydoc IInstructionTranslator::getRegIdx
    int getRegIdx(const QString& name) const override;

protected:

    /**
     * \brief   Given an instruction name and a variable list of expressions representing the actual operands of
     *              the instruction, use the RTL template dictionary to return the instantiated RTL representing the
     *              semantics of the instruction. This method also displays a disassembly of the instruction if the
     *              relevant compilation flag has been set.
     *
     * \param   pc  native PC
     * \param   name - instruction name
     * \param   args Semantic String ptrs representing actual operands
     * \returns an instantiated list of Exps
     */
    std::list<Statement *> *instantiate(Address pc, const char *name, const std::initializer_list<SharedExp>& args = {});

    /**
     * \brief   Similarly to NJMCDecoder::instantiate, given a parameter name and a list of Exp*'s representing
     * sub-parameters, return a fully substituted Exp for the whole expression
     * \note    Caller must delete result
     * \param   name - parameter name
     *          ... - Exp* representing actual operands
     * \returns an instantiated list of Exps
     */
    SharedExp instantiateNamedParam(char *name, const std::initializer_list<SharedExp>& args);

    /**
     * \brief   In the event that it's necessary to synthesize the call of a named parameter generated with
     *          instantiateNamedParam(), this substituteCallArgs() will substitute the arguments that follow into
     *          the expression.
     *
     * \note    Should only be used after instantiateNamedParam(name, ..);
     * \note    exp (the pointer) could be changed
     *
     * \param   name - parameter name
     * \param   exp - expression to instantiate into
     * \param   args Exp* representing actual operands
     */
    void substituteCallArgs(char *name, SharedExp *exp, const std::initializer_list<SharedExp>& args);

    /**
     * \brief   Process an unconditional jump instruction
     *              Also check if the destination is a label (MVE: is this done?)
     * \param   name name of instruction (for debugging)
     * \param   size size of instruction in bytes
     * \param   pc native pc
     * \param   stmts list of statements (?)
     * \param   result ref to decoder result object
     */
    void processUnconditionalJump(const char *name, int size, HostAddress relocd, ptrdiff_t delta, Address pc,
                                  std::list<Statement *> *stmts, DecodeResult& result);


    /**
     * \brief   Converts a numbered register to a suitable expression.
     * \param   regNum - the register number, e.g. 0 for eax
     * \returns the Exp* for the register NUMBER (e.g. "int 36" for %f4)
     */
    SharedExp dis_Reg(int regNum);

    /**
     * \brief        Converts a number to a Exp* expression.
     * \param        num - a number
     * \returns      the Exp* representation of the given number
     */
    SharedExp dis_Num(unsigned num);

protected:
    // Dictionary of instruction patterns, and other information summarised from the SSL file
    // (e.g. source machine's endianness)
    RTLInstDict m_rtlDict;
    Prog *m_prog;
    IBinaryImage *m_image;
};


/**
 * Function used to guess whether a given pc-relative address is the start of a function
 * Does the instruction at the given offset correspond to a caller prologue?
 * \note Implemented in the decoder.m files
 */
bool isFuncPrologue(Address hostPC);

/**
 * These are the macros that each of the .m files depend upon.
 */

#define SHOW_ASM(output)               \
    if (DEBUG_DECODER) {               \
        QString asmStr;                \
        QTextStream ost(&asmStr);      \
        ost << output;                 \
        LOG_MSG("%1: %2", pc, asmStr); \
    }

/*
 * addresstoPC returns the raw number as the address.  PC could be an
 * abstract type, in our case, PC is the raw address.
 */
#define addressToPC(pc)    pc

// Macros for branches. Note: don't put inside a "match" statement, since
// the ordering is changed and multiple copies may be made

#define COND_JUMP(name, size, relocd, cond)                                    \
    result.rtl = new RTL(pc, stmts);                                           \
    BranchStatement *jump = new BranchStatement;                               \
    result.rtl->append(jump);                                                  \
    result.numBytes = size;                                                    \
    jump->setDest(Address(relocd.value() - Util::signExtend<int64_t>(delta))); \
    jump->setCondType(cond);                                                   \
    SHOW_ASM(name << " " << relocd)

// This one is X86 specific
#define SETS(name, dest, cond)           \
    BoolAssign * bs = new BoolAssign(8); \
    bs->setLeftFromList(stmts);          \
    stmts->clear();                      \
    result.rtl = new RTL(pc, stmts);     \
    result.rtl->append(bs);              \
    bs->setCondType(cond);               \
    result.numBytes = 3;                 \
    SHOW_ASM(name << " " << dest)

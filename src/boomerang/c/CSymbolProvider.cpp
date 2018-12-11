#pragma region License
/*
 * This file is part of the Boomerang Decompiler.
 *
 * See the file "LICENSE.TERMS" for information on usage and
 * redistribution of this file, and for a DISCLAIMER OF ALL
 * WARRANTIES.
 */
#pragma endregion License
#include "CSymbolProvider.h"

#include "boomerang/c/parser/AnsiCParserDriver.h"
#include "boomerang/db/Prog.h"
#include "boomerang/db/binary/BinarySymbol.h"
#include "boomerang/db/binary/BinarySymbolTable.h"
#include "boomerang/db/module/Module.h"
#include "boomerang/db/proc/Proc.h"
#include "boomerang/ifc/IFrontEnd.h"
#include "boomerang/util/log/Log.h"

#include <QDir>
#include <QFileInfo>
#include <QTextStream>


CSymbolProvider::CSymbolProvider(Prog *prog)
    : m_prog(prog)
{
}


bool CSymbolProvider::readLibraryCatalog(const QString &filePath)
{
    // TODO: this is a work for generic semantics provider plugin : HeaderReader
    QFile file(filePath);

    if (!file.open(QFile::ReadOnly | QFile::Text)) {
        LOG_ERROR("Cannot open library signature catalog `%1'", filePath);
        return false;
    }

    QTextStream is(&file);

    while (!is.atEnd()) {
        QString sigFilePath;
        is >> sigFilePath;
        sigFilePath = sigFilePath.mid(0, sigFilePath.indexOf('#')); // cut the line to first '#'

        if ((sigFilePath.size() > 0) && sigFilePath.endsWith('\n')) {
            sigFilePath = sigFilePath.mid(0, sigFilePath.size() - 1);
        }

        if (sigFilePath.isEmpty()) {
            continue;
        }

        CallConv cc = CallConv::C; // Most APIs are C calling convention

        if (sigFilePath == "windows.h") {
            cc = CallConv::Pascal; // One exception
        }

        if (sigFilePath == "mfc.h") {
            cc = CallConv::ThisCall; // Another exception
        }

        const QString sig_path = QFileInfo(filePath).absoluteDir().absoluteFilePath(sigFilePath);
        if (!readLibrarySignatures(qPrintable(sig_path), cc)) {
            return false;
        }
    }

    return true;
}


bool CSymbolProvider::readLibrarySignatures(const QString &signatureFile, CallConv cc)
{
    AnsiCParserDriver driver;
    if (driver.parse(signatureFile, m_prog->getMachine(), cc) != 0) {
        LOG_ERROR("Cannot read library signature file '%1'", signatureFile);
        return false;
    }

    for (std::shared_ptr<Signature> &signature : driver.signatures) {
        m_librarySignatures[signature->getName()] = signature;
        signature->setSigFilePath(signatureFile);
    }

    return true;
}


bool CSymbolProvider::addSymbolsFromSymbolFile(const QString &fname)
{
    AnsiCParserDriver driver;
    const CallConv cc = m_prog->isWin32() ? CallConv::Pascal : CallConv::C;

    if (driver.parse(fname, m_prog->getMachine(), cc) != 0) {
        LOG_ERROR("Cannot read symbol file '%1': %2", fname);
        return false;
    }

    Module *targetModule = m_prog->getRootModule();

    for (std::shared_ptr<Symbol> &sym : driver.symbols) {
        if (sym->sig) {
            QString name = sym->sig->getName();
            targetModule = m_prog->getOrInsertModuleForSymbol(name);
            auto bin_sym = m_prog->getBinaryFile()->getSymbols()->findSymbolByAddress(sym->addr);
            const bool isLib = (bin_sym && bin_sym->isImportedFunction()) ||
                               // NODECODE isn't really the right modifier; perhaps we should have a
                               // LIB modifier, to specifically specify that this function obeys
                               // library calling conventions
                               sym->mods->noDecode;
            Function *p = targetModule->createFunction(name, sym->addr, isLib);

            if (!sym->mods->incomplete) {
                p->setSignature(sym->sig->clone());
                p->getSignature()->setForced(true);
            }
        }
        else {
            QString name  = sym->name;
            SharedType ty = sym->ty;

            m_prog->createGlobal(sym->addr, sym->ty, sym->name);
        }
    }

    for (std::shared_ptr<SymbolRef> &ref : driver.refs) {
        m_prog->getFrontEnd()->addRefHint(ref->addr, ref->name);
    }

    return true;
}


std::shared_ptr<Signature> CSymbolProvider::getSignatureByName(const QString &functionName) const
{
    auto it = m_librarySignatures.find(functionName);
    return it != m_librarySignatures.end() ? it.value() : nullptr;
}

#include "DecompilerThread.h"

#include "boomerang.h"
#include "log.h"
#include "prog.h"
#include "frontend.h"
#include "proc.h"
#include "signature.h"
#include "cluster.h"

#include <QtWidgets>
#include <QtCore>
#include <QThread>
#include <QString>
#include <QTableWidget>
#include <sstream>

#undef NO_ADDRESS
#define NO_ADDRESS ADDRESS::g(-1)

Qt::HANDLE threadToCollect = 0;

// void* operator new(size_t n) {
//    Qt::HANDLE curThreadId = QThread::currentThreadId();
//    if (curThreadId == threadToCollect)
//        return GC_malloc(n);
//    else
//        return GC_malloc_uncollectable(n);    // Don't collect, but mark
//}

// void operator delete(void* p) {
//    Qt::HANDLE curThreadId = QThread::currentThreadId();
//    if (curThreadId != threadToCollect)
//        GC_free(p); // Important to call this if you call GC_malloc_uncollectable
//}

void DecompilerThread::run() {
    threadToCollect = QThread::currentThreadId();

    Boomerang::get()->setOutputDirectory(".\\output\\");
    // Boomerang::get()->vFlag = true;
    // Boomerang::get()->traceDecoder = true;

    Parent = new Decompiler();
    Parent->moveToThread(this);

    Boomerang::get()->addWatcher(Parent);

    this->setPriority(QThread::LowPriority);

    exec();
}

Decompiler *DecompilerThread::getDecompiler() {
    while (Parent == nullptr)
        msleep(10);
    return Parent;
}

void Decompiler::setUseDFTA(bool d) { Boomerang::get()->dfaTypeAnalysis = d; }

void Decompiler::setNoDecodeChildren(bool d) { Boomerang::get()->noDecodeChildren = d; }

void Decompiler::addEntryPoint(ADDRESS a, const char *nam) {
    user_entrypoints.push_back(a);
    fe->AddSymbol(a, nam);
}

void Decompiler::removeEntryPoint(ADDRESS a) {
    for (std::vector<ADDRESS>::iterator it = user_entrypoints.begin(); it != user_entrypoints.end(); it++)
        if (*it == a) {
            user_entrypoints.erase(it);
            break;
        }
}

void Decompiler::changeInputFile(const QString &f) { filename = f; }

void Decompiler::changeOutputPath(const QString &path) { Boomerang::get()->setOutputDirectory(qPrintable(path)); }

void Decompiler::load() {
    emit loading();

    prog = new Prog();
    fe = FrontEnd::Load(filename, prog);
    if (fe == NULL) {
        emit machineType(QString("unavailable: Load Failed!"));
        return;
    }
    prog->setFrontEnd(fe);
    fe->readLibraryCatalog();

    switch (prog->getMachine()) {
    case MACHINE_PENTIUM:
        emit machineType(QString("pentium"));
        break;
    case MACHINE_SPARC:
        emit machineType(QString("sparc"));
        break;
    case MACHINE_HPRISC:
        emit machineType(QString("hprisc"));
        break;
    case MACHINE_PALM:
        emit machineType(QString("palm"));
        break;
    case MACHINE_PPC:
        emit machineType(QString("ppc"));
        break;
    case MACHINE_ST20:
        emit machineType(QString("st20"));
        break;
    case MACHINE_MIPS:
        emit machineType(QString("mips"));
        break;
    case MACHINE_68K:
        emit machineType(QString("m68k"));
        break;
    }

    QStringList entrypointStrings;
    std::vector<ADDRESS> entrypoints = fe->getEntryPoints();
    for (unsigned int i = 0; i < entrypoints.size(); i++) {
        user_entrypoints.push_back(entrypoints[i]);
        emit newEntrypoint(entrypoints[i], prog->symbolByAddress(entrypoints[i]));
    }

    for (int i = 1; i < fe->getSectionIface()->GetNumSections(); i++) {
        PSectionInfo section = fe->getSectionIface()->GetSectionInfo(i);
        emit newSection(section->pSectionName, section->uNativeAddr, section->uNativeAddr + section->uSectionSize);
    }

    emit loadCompleted();
}

void Decompiler::decode() {
    emit decoding();

    bool gotMain;
    ADDRESS a = fe->getMainEntryPoint(gotMain);
    for (unsigned int i = 0; i < user_entrypoints.size(); i++)
        if (user_entrypoints[i] == a) {
            fe->decode(prog, true, NULL);
            break;
        }

    for (unsigned int i = 0; i < user_entrypoints.size(); i++) {
        prog->decodeEntryPoint(user_entrypoints[i]);
    }

    if (!Boomerang::get()->noDecodeChildren) {
        // decode anything undecoded
        fe->decode(prog, NO_ADDRESS);
    }

    prog->finishDecode();

    emit decodeCompleted();
}

void Decompiler::decompile() {
    emit decompiling();

    prog->decompile();

    emit decompileCompleted();
}

void Decompiler::emitClusterAndChildren(Cluster *root) {
    emit newCluster(root->getName());
    for (unsigned int i = 0; i < root->getNumChildren(); i++)
        emitClusterAndChildren(root->getChild(i));
}

void Decompiler::generateCode() {
    emit generatingCode();

    prog->generateCode();

    Cluster *root = prog->getRootCluster();
    if (root)
        emitClusterAndChildren(root);
    std::list<Function *>::iterator it;
    for (UserProc *p = prog->getFirstUserProc(it); p; p = prog->getNextUserProc(it)) {
        emit newProcInCluster(p->getName(), p->getCluster()->getName());
    }

    emit generateCodeCompleted();
}

const char *Decompiler::procStatus(UserProc *p) {
    switch (p->getStatus()) {
    case PROC_UNDECODED:
        return "undecoded";
    case PROC_DECODED:
        return "decoded";
    case PROC_SORTED:
        return "sorted";
    case PROC_VISITED:
        return "visited";
    case PROC_INCYCLE:
        return "in cycle";
    case PROC_PRESERVEDS:
        return "preserveds";
    case PROC_EARLYDONE:
        return "early done";
    case PROC_FINAL:
        return "final";
    case PROC_CODE_GENERATED:
        return "code generated";
    }
    return "unknown";
}

void Decompiler::alertConsidering(Function *parent, Function *p) {
    emit consideringProc(QString(parent ? parent->getName() : ""), QString(p->getName()));
}

void Decompiler::alertDecompiling(UserProc *p) { emit decompilingProc(QString(p->getName())); }

void Decompiler::alertNew(Function *p) {
    if (p->isLib()) {
        QString params;
        if (p->getSignature() == NULL || p->getSignature()->isUnknown())
            params = "<unknown>";
        else {
            for (unsigned int i = 0; i < p->getSignature()->getNumParams(); i++) {
                Type *ty = p->getSignature()->getParamType(i);
                params.append(ty->getCtype());
                params.append(" ");
                params.append(p->getSignature()->getParamName(i));
                if (i != p->getSignature()->getNumParams() - 1)
                    params.append(", ");
            }
        }
        emit newLibProc(QString(p->getName()), params);
    } else {
        emit newUserProc(QString(p->getName()), p->getNativeAddress());
    }
}

void Decompiler::alertRemove(Function *p) {
    if (p->isLib()) {
        emit removeLibProc(QString(p->getName()));
    } else {
        emit removeUserProc(QString(p->getName()), p->getNativeAddress());
    }
}

void Decompiler::alertUpdateSignature(Function *p) { alertNew(p); }

bool Decompiler::getRtlForProc(const QString &name, QString &rtl) {
    Function *p = prog->findProc(name);
    if (p->isLib())
        return false;
    UserProc *up = (UserProc *)p;
    QTextStream os(&rtl);
    up->print(os, true);
    return true;
}

void Decompiler::alertDecompileDebugPoint(UserProc *p, const char *description) {
    LOG << p->getName() << ": " << description << "\n";
    if (Debugging) {
        Waiting = true;
        emit debuggingPoint(QString(p->getName()), QString(description));
        while (Waiting) {
            thread()->wait(10);
        }
    }
}

void Decompiler::stopWaiting() { Waiting = false; }

const char *Decompiler::getSigFile(const QString &name) {
    Function *p = prog->findProc(name);
    if (p == NULL || !p->isLib() || p->getSignature() == NULL)
        return NULL;
    return p->getSignature()->getSigFile();
}

QString Decompiler::getClusterFile(const QString &name) {
    Cluster *c = prog->findCluster(name);
    if (c == NULL)
        return "";
    return c->getOutPath("c");
}

void Decompiler::rereadLibSignatures() { prog->rereadLibSignatures(); }

void Decompiler::renameProc(const QString &oldName, const QString &newName) {
    Function *p = prog->findProc(oldName);
    if (p)
        p->setName(newName);
}

void Decompiler::getCompoundMembers(const QString &name, QTableWidget *tbl) {
    Type *ty = NamedType::getNamedType(name);
    tbl->setRowCount(0);
    if (ty == NULL || !ty->resolvesToCompound())
        return;
    CompoundType *c = ty->asCompound();
    for (unsigned int i = 0; i < c->getNumTypes(); i++) {
        tbl->setRowCount(tbl->rowCount() + 1);
        tbl->setItem(tbl->rowCount() - 1, 0, new QTableWidgetItem(tr("%1").arg(c->getOffsetTo(i))));
        tbl->setItem(tbl->rowCount() - 1, 1, new QTableWidgetItem(tr("%1").arg(c->getOffsetTo(i) / 8)));
        tbl->setItem(tbl->rowCount() - 1, 2, new QTableWidgetItem(QString(c->getName(i))));
        tbl->setItem(tbl->rowCount() - 1, 3, new QTableWidgetItem(tr("%1").arg(c->getType(i)->getSize())));
    }
}
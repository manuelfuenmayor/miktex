/**
 * @file cfg.cpp
 * @author Christian Schenk
 * @brief MiKTeX cfg utility
 *
 * @copyright Copyright © 2006-2022 Christian Schenk
 *
 * This file is part of Cfg.
 *
 * Cfg is licensed under GNU General Public License version 2 or any later
 * version.
 */

#include <cstdio>
#include <cstdlib>

#if defined(MIKTEX_WINDOWS)
#include <conio.h>
#else
#include <termios.h>
#include <unistd.h>
#endif

#include <miktex/Core/Cfg>
#include <miktex/Core/Exceptions>
#include <miktex/Core/MD5>
#include <miktex/Core/Session>
#include <miktex/Util/StringUtil>
#include <miktex/Wrappers/PoptWrapper>

#include "cfg-version.h"

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <locale>

using namespace MiKTeX::Core;
using namespace MiKTeX::Util;
using namespace MiKTeX::Wrappers;
using namespace std;

#define T_(x) MIKTEXTEXT(x)

enum class TASK { None, ComputeDigest, PrintClasses, SetValue, Sign };

enum Option
{
    OPT_AAA = 1000,
    OPT_COMPUTE_DIGEST,
    OPT_PRINT_CLASSES,
    OPT_PRIVATE_KEY_FILE,
    OPT_SET_VALUE,
    OPT_SIGN,
    OPT_VERSION,
};

const struct poptOption aoption[] =
{
    {
        "compute-digest", 0, POPT_ARG_NONE, nullptr, OPT_COMPUTE_DIGEST,
        T_("Compute the MD5."), nullptr,
    },
    {
        "print-classes", 0, POPT_ARG_NONE, nullptr, OPT_PRINT_CLASSES,
        T_("Print C++ class definitions."), nullptr,
    },
    {
        "private-key-file", 0, POPT_ARG_STRING, nullptr, OPT_PRIVATE_KEY_FILE,
        T_("The private key file used for signing."), T_("FILE"),
    },
    {
        "set-value", 0, POPT_ARG_STRING, nullptr, OPT_SET_VALUE,
        T_("Sets a value."), T_("NAME=VALUE"),
    },
    {
        "sign", 0, POPT_ARG_NONE, nullptr, OPT_SIGN,
        T_("Sign the cfg file."), nullptr,
    },
    {
        T_("version"), 0, POPT_ARG_NONE, nullptr, OPT_VERSION,
        T_("Show version information and exit."), nullptr
    },
    POPT_AUTOHELP
    POPT_TABLEEND
};

void FatalError(const string& msg)
{
    cerr << Utils::GetExeName() << ": " << msg << endl;
    throw 1;
}

void PrintDigest(const MD5& md5)
{
    cout << md5 << endl;
}

string ToStr(const string& s)
{
    string result;
    for (char ch : s)
    {
        switch (ch)
        {
        case '\\':
            result += "\\\\";
            break;
        case '"':
            result += "\\\"";
            break;
        default:
            result += ch;
            break;
        }
    }
    return result;
}

void DoPrintClasses(Cfg& cfg)
{
    for (const shared_ptr<Cfg::Key>& key : cfg)
    {
        cout << "class " << key->GetName() << " {" << endl;
        for (const shared_ptr<Cfg::Value>& val : *key)
        {
            string value = val->AsString();
            char* endptr = nullptr;
            auto ignored = strtol(value.c_str(), &endptr, 0);
            bool isNumber = endptr == nullptr || *endptr == 0;
            cout << "  public: static ";
            if (isNumber)
            {
                cout << "int";
            }
            else
            {
                cout << "std::string";
            }
            cout
                << " " << val->GetName()
                << "() { return ";
            if (isNumber)
            {
                cout << value;
            }
            else
            {
                cout << '"' << ToStr(value) << '"';
            }
            cout << "; }" << endl;
        }
        cout << "};" << endl;
    }
}

class PrivateKeyProvider :
    public IPrivateKeyProvider
{
public:

    PrivateKeyProvider(const PathName& privateKeyFile) :
        privateKeyFile(privateKeyFile)
    {
    }

    PathName MIKTEXTHISCALL GetPrivateKeyFile() override
    {
        return privateKeyFile;
    }

    bool GetPassphrase(std::string& passphrase) override
    {
        cout << T_("Passphrase: ");
#if defined(MIKTEX_WINDOWS)
        const int EOL = '\r';
        CharBuffer<wchar_t> buf;
        wint_t ch;
        while ((ch = _getwch()) != EOL)
        {
            buf += ch;
        }
        passphrase = StringUtil::WideCharToUTF8(buf.GetData());
        return true;
#else
        struct termios tty;
        if (tcgetattr(STDIN_FILENO, &tty) != 0) {
            MIKTEX_FATAL_CRT_ERROR("tcgetattr");
        }
        tty.c_lflag &= ~ECHO;
        if (tcsetattr(STDIN_FILENO, TCSANOW, &tty) != 0) {
            MIKTEX_FATAL_CRT_ERROR("tcsetattr");
        }
        getline(cin, passphrase);
        tty.c_lflag |= ECHO;
        if (tcsetattr(STDIN_FILENO, TCSANOW, &tty) != 0) {
            MIKTEX_FATAL_CRT_ERROR("tcsetattr");
        }
#endif
        cout << endl;
        return true;
    }

private:

    PathName privateKeyFile;
};

void Main(int argc, const char** argv)
{
    int option;

    PoptWrapper popt(argc, argv, aoption);

    popt.SetOtherOptionHelp(T_("[OPTION...] CFGFILE..."));

    TASK task = TASK::ComputeDigest;

    PathName privateKeyFile;
    vector<pair<string, string>> values;

    while ((option = popt.GetNextOpt()) >= 0)
    {
        string optArg = popt.GetOptArg();
        switch (option)
        {
        case OPT_COMPUTE_DIGEST:
            task = TASK::ComputeDigest;
            break;
        case OPT_PRINT_CLASSES:
            task = TASK::PrintClasses;
            break;
        case OPT_PRIVATE_KEY_FILE:
            privateKeyFile = optArg;
            break;
        case OPT_SET_VALUE:
        {
            task = TASK::SetValue;
            size_t pos = optArg.find('=');
            if (pos == string::npos)
            {
                FatalError("bad value");
            }
            values.push_back(make_pair<string, string>(optArg.substr(0, pos), optArg.substr(pos + 1)));
            break;
        }
        case OPT_SIGN:
            task = TASK::Sign;
            break;
        case OPT_VERSION:
            cout
                << Utils::MakeProgramVersionString(Utils::GetExeName(), VersionNumber(MIKTEX_COMP_MAJOR_VERSION, MIKTEX_COMP_MINOR_VERSION, MIKTEX_COMP_PATCH_VERSION, 0)) << "\n"
                << T_("Copyright (C) 2006-2022 Christian Schenk") << "\n"
                << T_("This is free software; see the source for copying conditions.  There is NO") << "\n"
                << T_("warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.") << endl;
            return;
        }
    }

    if (option != -1)
    {
        string msg = popt.BadOption(POPT_BADOPTION_NOALIAS);
        msg += ": ";
        msg += popt.Strerror(option);
        FatalError(msg);
    }

    vector<string> leftovers = popt.GetLeftovers();

    if (leftovers.empty())
    {
        FatalError(T_("no file name arguments"));
    }

    for (const string& fileName : leftovers)
    {
        unique_ptr<Cfg> pCfg(Cfg::Create());
        pCfg->Read(PathName(fileName));
        if (task == TASK::ComputeDigest)
        {
            PrintDigest(pCfg->GetDigest());
        }
        else if (task == TASK::PrintClasses)
        {
            DoPrintClasses(*pCfg);
        }
        else if (task == TASK::Sign)
        {
            PrivateKeyProvider privateKeyProvider(privateKeyFile);
            pCfg->Write(PathName(fileName), "", &privateKeyProvider);
        }
        else if (task == TASK::SetValue)
        {
            for (const pair<string, string>& nv : values)
            {
                pCfg->PutValue("", nv.first, nv.second);
            }
            pCfg->Write(PathName(fileName), "");
        }
    }
}

int main(int argc, const char** argv)
{
    int exitCode;
    try
    {
        shared_ptr<Session> session = Session::Create(Session::InitInfo(argv[0]));
        Main(argc, argv);
        exitCode = 0;
    }
    catch (const MiKTeXException& e)
    {
        Utils::PrintException(e);
        exitCode = 1;
    }
    catch (const exception& e)
    {
        Utils::PrintException(e);
        exitCode = 1;
    }
    catch (int r)
    {
        exitCode = r;
    }
    return exitCode;
}

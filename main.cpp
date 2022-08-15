#include <QDir>

#include <csignal>

#include <execinfo.h>
#include <unistd.h>

#include "Application.hpp"

void systemSignalsHandler(int signalNumber);

int main(int argc, char** argv)
{
    signal(SIGTERM, systemSignalsHandler);
    signal(SIGINT , systemSignalsHandler);
    signal(SIGABRT, systemSignalsHandler);
    signal(SIGSEGV, systemSignalsHandler);

    qSetMessagePattern("[%{time h:mm:ss:zzz}]["
                       "%{if-debug}D%{endif}"
                       "%{if-info}I%{endif}"
                       "%{if-warning}W%{endif}"
                       "%{if-critical}C%{endif}"
                       "%{if-fatal}F%{endif}] "
                       "%{message}");

    {// app path set
        char result[PATH_MAX];
        const auto count = readlink("/proc/self/exe", result, PATH_MAX);
        const auto string = QString::fromLatin1(result, count);

        QDir::setCurrent(string.left(string.lastIndexOf('/')));
    }

    {
        if (geteuid() == 0) qInfo("Superuser permissions granted!");
        else
        {
            qInfo("This program needs to be started with superuser permissions, "
                  "e.g. 'sudo %s [options]'!", basename(argv[0]));
            return -1;
        }
    }

    const Application application(argc, argv);
    return application.exec();
}

void systemSignalsHandler(int signalNumber)
{
    static bool accepted = false;

    if (!accepted)
    {
        qInfo("System signal accepted: %i", signalNumber);
        accepted = true;

        switch (signalNumber)
        {
            case SIGINT:
            case SIGTERM: signalNumber = 0;
            break;
            default:
            {
                char **strings;
                size_t i, size;
                enum Constexpr { MAX_SIZE = 1024 };
                void *array[MAX_SIZE];
                size = backtrace(array, MAX_SIZE);
                strings = backtrace_symbols(array, size);
                for (i = 0; i < size; i++)
                    fprintf(stderr, "%s\n", strings[i]);
                puts("");
                free(strings);
            }
        }

        qApp->exit(signalNumber);
    }
}
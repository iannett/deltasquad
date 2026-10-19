// port code: replaces the xbox file manager. not part of the decompilation.

#include <cstdio>
#include <cerrno>
#include <cstdlib>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <unistd.h>
#include <string>

#include "Core.h"

FFileManager* GFileManager;

namespace
{
    void MakeDirectoryTree(const std::string& Path)
    {
        for (size_t Slash = Path.find('/', 1); Slash != std::string::npos; Slash = Path.find('/', Slash + 1))
            mkdir(Path.substr(0, Slash).c_str(), 0755);
        mkdir(Path.c_str(), 0755);
    }

    // host folder for an xbox drive, with a trailing slash. empty means relative to the working directory.
    std::string DriveRoot(char Drive)
    {
        if (Drive >= 'a' && Drive <= 'z')
            Drive -= 'a' - 'A';
        if (Drive == 'D')
        {
            // the disc is the parent of the base (System) directory
            std::string Base(appBaseDir());
            while (!Base.empty() && Base.back() == '/')
                Base.pop_back();
            const size_t Slash = Base.rfind('/');
            return Slash == std::string::npos ? std::string() : Base.substr(0, Slash + 1);
        }
        if (Drive == 'T' || Drive == 'U' || Drive == 'Z')
        {
            const char* Home = getenv("HOME");
            if (!Home)
                return std::string();
            const std::string Root = std::string(Home) + "/Library/Application Support/deltasquad/" + Drive;
            MakeDirectoryTree(Root);
            return Root + "/";
        }
        return std::string();
    }

    // xbox drive paths: D: is the game folder, T:/U:/Z: live in the per-user folder
    std::string ToHostPath(const TCHAR* Filename)
    {
        std::string Path(Filename);
        if (Path.size() >= 2 && Path[1] == ':')
        {
            const std::string Root = DriveRoot(Path[0]);
            Path.erase(0, 2);
            while (!Path.empty() && (Path[0] == '\\' || Path[0] == '/'))
                Path.erase(0, 1);
            Path = Root + Path;
            if (Path.empty())
                Path = ".";
        }
        for (char& Ch : Path)
        {
            if (Ch == '\\')
                Ch = '/';
        }
        return Path;
    }

    class FArchiveFileReaderPosix : public FArchive
    {
    public:
        FArchiveFileReaderPosix(FILE* InFile, INT InSize)
            : File(InFile)
            , Size(InSize)
        {
            ArIsLoading = true;
            ArIsPersistent = true;
        }
        ~FArchiveFileReaderPosix() override { fclose(File); }

        void Serialize(void* V, INT Length) override
        {
            if (Length && fread(V, Length, 1, File) != 1)
            {
                ArIsError = true;
                GError->Logf(TEXT("ReadFile failed: %i bytes at %i of %i"), Length, Tell(), Size);
            }
        }
        INT Tell() override { return static_cast<INT>(ftell(File)); }
        INT TotalSize() override { return Size; }
        void Seek(INT Pos) override { fseek(File, Pos, SEEK_SET); }

    private:
        FILE* File;
        INT   Size;
    };

    class FArchiveFileWriterPosix : public FArchive
    {
    public:
        explicit FArchiveFileWriterPosix(FILE* InFile)
            : File(InFile)
        {
            ArIsSaving = true;
            ArIsPersistent = true;
        }
        ~FArchiveFileWriterPosix() override { fclose(File); }

        void Serialize(void* V, INT Length) override
        {
            if (Length && fwrite(V, Length, 1, File) != 1)
                ArIsError = true;
        }
        INT Tell() override { return static_cast<INT>(ftell(File)); }
        void Seek(INT Pos) override { fseek(File, Pos, SEEK_SET); }

    private:
        FILE* File;
    };

    class FFileManagerPosix : public FFileManager
    {
    public:
        FArchive* CreateFileReader(const TCHAR* Filename, DWORD ReadFlags, FOutputDevice* Error) override
        {
            FILE* File = fopen(ToHostPath(Filename).c_str(), "rb");
            if (!File)
            {
                if (ReadFlags & FILEREAD_NoFail)
                    GError->Logf(TEXT("Failed to read file: %s"), Filename);
                return nullptr;
            }
            fseek(File, 0, SEEK_END);
            const INT Size = static_cast<INT>(ftell(File));
            fseek(File, 0, SEEK_SET);
            return new FArchiveFileReaderPosix(File, Size);
        }

        FArchive* CreateFileWriter(const TCHAR* Filename, DWORD WriteFlags, FOutputDevice* Error) override
        {
            FILE* File = fopen(ToHostPath(Filename).c_str(), (WriteFlags & FILEWRITE_Append) ? "ab" : "wb");
            if (!File)
            {
                if (WriteFlags & FILEWRITE_NoFail)
                    GError->Logf(TEXT("Failed to write file: %s"), Filename);
                return nullptr;
            }
            return new FArchiveFileWriterPosix(File);
        }

        INT FileSize(const TCHAR* Filename) override
        {
            FILE* File = fopen(ToHostPath(Filename).c_str(), "rb");
            if (!File)
                return -1;
            fseek(File, 0, SEEK_END);
            const INT Size = static_cast<INT>(ftell(File));
            fclose(File);
            return Size;
        }

        SQWORD GetDiskFreeSpace(const TCHAR* Path) override
        {
            const std::string HostPath = ToHostPath(Path);
            struct statvfs Stats;
            if (statvfs(HostPath.c_str(), &Stats) != 0)
                return 0;
            return static_cast<SQWORD>(Stats.f_bavail) * static_cast<SQWORD>(Stats.f_frsize);
        }

        UBOOL MakeDirectory(const TCHAR* Path, UBOOL Tree) override
        {
            std::string HostPath = ToHostPath(Path);
            while (!HostPath.empty() && HostPath.back() == '/')
                HostPath.pop_back();
            if (HostPath.empty() || HostPath == ".")
                return 1;
            if (Tree)
                MakeDirectoryTree(HostPath);
            return mkdir(HostPath.c_str(), 0755) == 0 || errno == EEXIST;
        }

        UBOOL SetDefaultDirectory(const TCHAR* Path) override
        {
            return !*Path || chdir(ToHostPath(Path).c_str()) == 0;
        }
    };
}

FFileManager* CreatePosixFileManager()
{
    static FFileManagerPosix FileManager;
    return &FileManager;
}

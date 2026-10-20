#include <cctype>
#include <cmath>
#include <cstring>

#include "Karma.h"

// ---------------------------------------------------------------------------------------------- files

// 0x00197ed0. a memory file reads only the whole elements it has left.
DWORD MeFileRead(void* Dest, DWORD ElementSize, DWORD Count, MeFile* File)
{
    if (!File)
        return 0;
    if (File->Type == 0)
        return static_cast<DWORD>(fread(Dest, ElementSize, Count, File->Handle));
    DWORD Bytes = ElementSize * Count;
    if (File->Pos + Bytes >= File->Size)
    {
        Count = static_cast<DWORD>(floor(static_cast<double>(File->Size - File->Pos) / static_cast<double>(ElementSize)));
        Bytes = Count * ElementSize;
    }
    memcpy(Dest, File->Buffer + File->Pos, Bytes);
    File->Pos += Bytes;
    return Count;
}

// 0x00197e90
void MeFileDestroy(MeFile* File)
{
    if (!File)
        return;
    if (File->Type == 0)
    {
        fclose(File->Handle);
        MeMemory.Destroy(File);
        return;
    }
    if (File->Buffer)
        MeMemory.Destroy(File->Buffer);
    MeMemory.Destroy(File);
}

// ---------------------------------------------------------------------------------------------- reading

namespace
{
    // 0x001ceb00
    char ReadChar(MeXMLInput* Input)
    {
        if (Input->EndOfFile)
            return 0;
        char Ch;
        if (Input->PushbackCount)
        {
            Ch = Input->Pushback[--Input->PushbackCount];
        }
        else if (Input->Pos >= Input->Length)
        {
            Input->Pos = 1;
            Input->Length = static_cast<INT>(MeFileRead(Input->Buffer, 1, sizeof(Input->Buffer), Input->File));
            if (!Input->Length)
            {
                Input->EndOfFile = 1;
                Input->Column++;
                return 0;
            }
            Ch = Input->Buffer[0];
        }
        else
        {
            Ch = Input->Buffer[Input->Pos++];
        }
        if (Ch == '\n')
        {
            Input->Line++;
            Input->Column = 1;
        }
        else
        {
            Input->Column++;
        }
        return Ch;
    }

    // 0x001cebf0. reads the text of the next tag, skipping comments and declarations. 0 when a tag was read,
    // 3 at the end of the file, otherwise an error with the message in Input->Error.
    INT ReadTag(MeXMLInput* Input, char* Tag, INT Size)
    {
        if (!Tag)
        {
            snprintf(Input->Error, sizeof(Input->Error), "line %d, character %d: Nowhere to put parsed data!\n", Input->Line, Input->Column);
            return 4;
        }
        char Ch;
        for (;;)
        {
            do
            {
                Ch = ReadChar(Input);
                if (Input->EndOfFile)
                    return 3;
            } while (Ch != '<');
            Ch = ReadChar(Input);
            if (Ch != '!')
                break;
            Ch = ReadChar(Input);
            if (Ch != '-')
            {
                // a declaration: skipped to its end
                do
                {
                    Ch = ReadChar(Input);
                    if (Input->EndOfFile)
                        break;
                } while (Ch != '>');
                continue;
            }
            // a comment, taken to start with "<!--"
            do
            {
                Ch = ReadChar(Input);
                if (Input->EndOfFile)
                {
                    snprintf(Input->Error, sizeof(Input->Error), "line %d, character %d: found EOF in comment\n", Input->Line, Input->Column);
                    return 1;
                }
            } while (Ch != '-' || ReadChar(Input) != '-');
            if (ReadChar(Input) != '>')
            {
                snprintf(Input->Error, sizeof(Input->Error), "line %d, character %d: found illegal '--' in comment\n", Input->Line, Input->Column);
                return 1;
            }
        }
        char* const End = Tag + Size - 1;
        for (;;)
        {
            *Tag++ = Ch;
            if (Tag == End)
            {
                snprintf(Input->Error, sizeof(Input->Error), "line %d, character %d: internal error: buffer overflow (tag too long)\n", Input->Line, Input->Column);
                return 4;
            }
            Ch = ReadChar(Input);
            if (Input->EndOfFile)
            {
                snprintf(Input->Error, sizeof(Input->Error), "line %d, character %d: found EOF inside tag\n", Input->Line, Input->Column);
                return 1;
            }
            if (Ch == '>')
            {
                *Tag = 0;
                return 0;
            }
        }
    }
}

namespace
{
    // 0x001ceba0
    void PushbackChar(MeXMLInput* Input, char Ch)
    {
        if (Input->PushbackCount < 4)
            Input->Pushback[Input->PushbackCount++] = Ch;
        if (Input->EndOfFile)
            Input->EndOfFile = 0;
        if (Ch == '\n')
        {
            Input->Line--;
            Input->Column = 0x7fff0000;
        }
        else
        {
            Input->Column--;
        }
    }

}

// 0x001ced60. reads the text before the next tag, skipping comments, and leaves the tag to be read.
INT MeXMLReadText(MeXMLInput* Input, char* Text, INT Size)
    {
        if (!Text)
        {
            snprintf(Input->Error, sizeof(Input->Error), "line %d, character %d: Nowhere to put parsed data!\n", Input->Line, Input->Column);
            return 4;
        }
        char* Out = Text;
        for (;;)
        {
            char Ch = ReadChar(Input);
            while (!Input->EndOfFile && Ch != '<')
            {
                *Out++ = Ch;
                if (Ch == '>')
                {
                    snprintf(Input->Error, sizeof(Input->Error), "line %d, character %d: unexpected '>'\n", Input->Line, Input->Column);
                    return 1;
                }
                if (Out == Text + Size - 1)
                {
                    snprintf(Input->Error, sizeof(Input->Error), "line %d, character %d: internal error: buffer overflow (data too long)\n", Input->Line, Input->Column);
                    return 4;
                }
                Ch = ReadChar(Input);
            }
            Ch = ReadChar(Input);
            if (Input->EndOfFile)
            {
                snprintf(Input->Error, sizeof(Input->Error), "line %d, character %d: found EOF inside tag\n", Input->Line, Input->Column);
                return 1;
            }
            if (Ch != '!')
            {
                PushbackChar(Input, Ch);
                if (Input->PushbackCount < 4)
                    Input->Pushback[Input->PushbackCount++] = '<';
                if (Input->EndOfFile)
                    Input->EndOfFile = 0;
                Input->Column--;
                *Out = 0;
                return 0;
            }
            // a comment, taken to start with "<!-". an unterminated one never ends: at the end of the file every
            // read returns 0.
            ReadChar(Input);
            for (;;)
            {
                Ch = ReadChar(Input);
                if (!Input->EndOfFile && Ch != '-')
                    continue;
                if (ReadChar(Input) != '-')
                    continue;
                break;
            }
            if (ReadChar(Input) != '>')
            {
                snprintf(Input->Error, sizeof(Input->Error), "line %d, character %d: found illegal '--' in comment\n", Input->Line, Input->Column);
                return 1;
            }
        }
    }


// 0x001cf260
MeXMLInput* MeXMLInputCreate(MeFile* File)
{
    MeXMLInput* Input = static_cast<MeXMLInput*>(MeMemory.Create(sizeof(MeXMLInput)));
    Input->File = File;
    Input->Pos = 0x1000;
    Input->Length = 0x1000;
    Input->Line = 1;
    Input->Column = 1;
    Input->PushbackCount = 0;
    Input->EndOfFile = 0;
    strcpy(Input->Error, "No error found");
    Input->UserData = nullptr;
    return Input;
}

// 0x001cf2f0
void MeXMLInputSetUserData(MeXMLInput* Input, void* UserData)
{
    Input->UserData = UserData;
}

// 0x001cf310
void* MeXMLInputGetUserData(MeXMLInput* Input)
{
    return Input->UserData;
}

// 0x001cf320
const char* MeXMLInputGetErrorString(MeXMLInput* Input)
{
    return Input->Error;
}

// 0x001cf390
MeXMLInput* MeXMLElementGetInput(MeXMLElement* Element)
{
    return Element->Input;
}

// 0x001cf330
INT MeXMLInputProcess(MeXMLInput* Input, MeXMLHandler* Handlers, void* Context)
{
    MeXMLElement Root;
    memset(Root.Tag, 0, sizeof(Root.Tag));
    Root.Input = Input;
    Root.Depth = 0;
    Root.Attributes = nullptr;
    return MeXMLParseElements(&Root, Handlers, nullptr, Context);
}

// 0x001cef20. handles each child of Parent with the handler for its tag until Parent's closing tag. unknown
// tags are skipped with their contents.
INT MeXMLParseElements(MeXMLElement* Parent, MeXMLHandler* Handlers, void* Data, void* Context)
{
    MeXMLInput* Input = Parent->Input;
    MeXMLHandler Skip[1] = {};
    Skip[0].Type = 2;
    const INT Line = Input->Line;
    const INT Column = Input->Column;
    for (;;)
    {
        MeXMLElement Child;
        INT Result = ReadTag(Input, Child.Tag, 0x100);
        if (Result == 3)
            return Parent->Depth ? 3 : 0;
        if (Result)
            return Result;
        if (Child.Tag[0] == '/')
        {
            if (Parent->Depth == 0)
            {
                snprintf(Input->Error, sizeof(Input->Error), "line %d, char %d: unexpected closing tag, found %s\n", Line, Column, Child.Tag);
                return 1;
            }
            if (strcmp(Child.Tag + 1, Parent->Tag) == 0)
                return 0;
            snprintf(Input->Error, sizeof(Input->Error), "line %d, char %d: closing tag %s doesn't match opening tag %s\n", Line, Column, Parent->Tag, Child.Tag);
            return 1;
        }
        if (Child.Tag[0] == '?')
            continue;

        Child.Input = Input;
        Child.Depth = Parent->Depth + 1;
        Child.Attributes = strchr(Child.Tag, ' ');
        if (Child.Attributes)
        {
            *Child.Attributes = 0;
            do
                Child.Attributes++;
            while (isspace(static_cast<unsigned char>(*Child.Attributes)));
        }

        MeXMLHandler* Handler = Handlers;
        while (Handler->Type != 2 && strcmp(Child.Tag, Handler->Tag) != 0)
            Handler++;
        if (Handler->Type == 0)
        {
            Handler->Found = 1;
            Result = reinterpret_cast<MeXMLElementFn>(Handler->Handler)(&Child, Context);
        }
        else if (Handler->Type == 2)
        {
            Result = MeXMLParseElements(&Child, Skip, nullptr, Context);
        }
        else
        {
            Handler->Found = 1;
            Result = reinterpret_cast<MeXMLDataFn>(Handler->Handler)(Input, Handler, Data);
            if (Result)
                return Result;
            if (Handler->Post)
            {
                Result = Handler->Post(&Child, Data, Context);
                if (Result)
                    return Result;
            }
            char Closing[1024];
            Result = ReadTag(Input, Closing, 0x100);
            if (Result)
                return Result;
            if (Closing[0] != '/' || strcmp(Closing + 1, Child.Tag) != 0)
            {
                snprintf(Input->Error, sizeof(Input->Error), "line %d, char %d: closing tag %s does not match opening tag %s", Line, Column, Closing, Child.Tag);
                return 1;
            }
        }
        if (Result)
            return Result;
    }
}

// ---------------------------------------------------------------------------------------------- trees

namespace
{
    // 0x001ce6c0. splits the next name="value" pair off Cursor in place.
    UBOOL ParseAttribute(char** Cursor, char** Value, char** Next)
    {
        char* Ch = *Cursor;
        while (isspace(static_cast<unsigned char>(*Ch)))
            Ch++;
        *Cursor = Ch;
        while (!isspace(static_cast<unsigned char>(*Ch)))
        {
            if (*Ch == '=')
                break;
            Ch++;
        }
        if (*Ch != '=')
            return 0;
        *Ch = 0;
        // either quote opens the value, but only a double quote closes it
        if (Ch[1] == '"' || Ch[1] == '\'')
        {
            *Value = Ch + 2;
            char* Close = strchr(Ch + 2, '"');
            if (Close)
            {
                *Close = 0;
                *Next = Close + 1;
                return 1;
            }
        }
        return 0;
    }

    char* CopyString(const char* Str)
    {
        char* Copy = static_cast<char*>(MeMemory.Create(static_cast<INT>(strlen(Str)) + 1));
        strcpy(Copy, Str);
        return Copy;
    }

    // 0x001ce810
    void AddAttribute(MeXMLNode* Node, const char* Name, const char* Value)
    {
        MeXMLAttribute* Attribute = static_cast<MeXMLAttribute*>(MeMemory.Create(sizeof(MeXMLAttribute)));
        MeXMLList* Entry = static_cast<MeXMLList*>(MeMemory.Create(sizeof(MeXMLList)));
        Attribute->Name = CopyString(Name);
        Attribute->Value = CopyString(Value);
        Entry->Item = Attribute;
        Entry->Next = Node->Attributes;
        Node->Attributes = Entry;
    }

    // 0x001ce8b0
    void ParseAttributes(MeXMLNode* Node, char* Attributes)
    {
        if (!Attributes || !*Attributes)
            return;
        char* Cursor = Attributes;
        char* Value;
        char* Next;
        while (ParseAttribute(&Cursor, &Value, &Next))
        {
            AddAttribute(Node, Cursor, Value);
            if (!*Next)
                break;
            Cursor = Next;
        }
    }
}

// 0x001cea20
MeXMLNode* MeXMLNodeCreate(INT Type, const void* Unused, void* Data, void (*DestroyData)(void*), char* Attributes)
{
    MeXMLNode* Node = static_cast<MeXMLNode*>(MeMemory.Create(sizeof(MeXMLNode)));
    Node->Type = Type;
    Node->Children = nullptr;
    Node->Attributes = nullptr;
    Node->Data = Data;
    Node->DestroyData = DestroyData;
    ParseAttributes(Node, Attributes);
    return Node;
}

// 0x001ce910. new children go first.
void MeXMLNodeAddChild(MeXMLNode* Child, MeXMLNode* Parent)
{
    MeXMLList* Entry = static_cast<MeXMLList*>(MeMemory.Create(sizeof(MeXMLList)));
    Entry->Item = Child;
    Entry->Next = Parent->Children;
    Parent->Children = Entry;
}

// 0x001ce940
const char* MeXMLNodeGetAttribute(MeXMLNode* Node, const char* Name)
{
    for (MeXMLList* Entry = Node->Attributes; Entry; Entry = Entry->Next)
    {
        MeXMLAttribute* Attribute = static_cast<MeXMLAttribute*>(Entry->Item);
        if (strcmp(Attribute->Name, Name) == 0)
            return Attribute->Value;
    }
    return nullptr;
}

// 0x001ce9b0
void MeXMLNodeWalk(MeXMLNode* Node, void (*Fn)(MeXMLNode* Node, MeXMLNode* Parent, void* Context), UBOOL PreOrder, void* Context)
{
    for (MeXMLList* Entry = Node->Children; Entry; Entry = Entry->Next)
    {
        MeXMLNode* Child = static_cast<MeXMLNode*>(Entry->Item);
        if (PreOrder)
            Fn(Child, Node, Context);
        MeXMLNodeWalk(Child, Fn, PreOrder, Context);
        if (!PreOrder)
            Fn(Child, Node, Context);
    }
}

// 0x001ce7e0
void MeXMLNodeDestroyChildren(MeXMLNode* Node, MeXMLNode* Parent, void* Context)
{
    MeXMLList* Entry = Node->Children;
    while (Entry)
    {
        MeXMLNodeDestroy(static_cast<MeXMLNode*>(Entry->Item));
        MeXMLList* Next = Entry->Next;
        MeMemory.Destroy(Entry);
        Entry = Next;
    }
}

// 0x001ce780
void MeXMLNodeDestroy(MeXMLNode* Node)
{
    MeXMLList* Entry = Node->Attributes;
    while (Entry)
    {
        MeXMLAttribute* Attribute = static_cast<MeXMLAttribute*>(Entry->Item);
        MeMemory.Destroy(Attribute->Name);
        MeMemory.Destroy(Attribute->Value);
        MeMemory.Destroy(Attribute);
        MeXMLList* Next = Entry->Next;
        MeMemory.Destroy(Entry);
        Entry = Next;
    }
    if (Node->Data && Node->DestroyData)
        Node->DestroyData(Node->Data);
    MeMemory.Destroy(Node);
}

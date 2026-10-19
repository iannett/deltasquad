using System.Text.Json;
using UELib;
using UELib.Core;

// usage: layout-dump <System dir> <output .json>
// writes every class and struct of every package with its properties in link order, and every native function,
// for tools/gen_layouts.py
var systemDir = args[0];
var outPath = args[1];
var packages = new List<object>();

foreach (var path in Directory.GetFiles(systemDir, "*.u").OrderBy(p => p))
{
    using var package = UnrealLoader.LoadPackage(path, FileAccess.Read);
    package.InitializePackage();
    var structs = new List<object>();
    foreach (var s in package.Objects.OfType<UStruct>().Where(s => (int)s > 0 && (s is UClass || s.GetType() == typeof(UScriptStruct) || s.GetType() == typeof(UStruct))))
    {
        structs.Add(new
        {
            name = s.Name.ToString(),
            outer = s.Outer?.Name.ToString(),
            kind = s is UClass ? "class" : "struct",
            super = s.Super?.Name.ToString(),
            superOuter = s.Super?.Outer?.Name.ToString(),
            classFlags = s is UClass c ? (ulong)c.ClassFlags : 0UL,
            structFlags = (ulong)s.StructFlags,
            properties = s.Variables.Select(Describe).ToList(),
        });
    }
    // native functions with the class that owns them (a function in a state belongs to the state's class)
    var natives = package.Objects.OfType<UFunction>()
        .Where(f => (int)f > 0 && f.HasFunctionFlag(UELib.Flags.FunctionFlags.Native))
        .Select(f => new
        {
            name = f.Name.ToString(),
            ownerClass = OwnerClass(f)?.Name.ToString(),
            outer = f.Outer?.Name.ToString(),
            native = (int)f.NativeToken,
            flags = (ulong)f.FunctionFlags,
        })
        .ToList();
    packages.Add(new { name = Path.GetFileNameWithoutExtension(path), structs, natives });
}

File.WriteAllText(outPath, JsonSerializer.Serialize(packages, new JsonSerializerOptions { WriteIndented = true }));

static UClass? OwnerClass(UObject o)
{
    while (o != null && o is not UClass)
        o = o.Outer;
    return o as UClass;
}

static object Describe(UProperty p) => new
{
    name = p.Name.ToString(),
    type = p.GetType().Name,
    flags = (ulong)p.PropertyFlags,
    arrayDim = p.ArrayDim,
    @enum = (p as UByteProperty)?.Enum?.Name.ToString(),
    objectClass = (p as UObjectProperty)?.Object?.Name.ToString(),
    metaClass = (p as UClassProperty)?.MetaClass?.Name.ToString(),
    structName = (p as UStructProperty)?.Struct?.Name.ToString(),
    structOuter = (p as UStructProperty)?.Struct?.Outer?.Name.ToString(),
    inner = p is UArrayProperty a && a.InnerProperty != null ? Describe(a.InnerProperty) : null,
};

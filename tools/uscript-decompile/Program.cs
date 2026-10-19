using UELib;
using UELib.Core;

// usage: uscript-decompile <System dir> <output dir>
var systemDir = args[0];
var outDir = args[1];
int classes = 0, failed = 0;

var paths = Directory.GetFiles(systemDir, "*.u").OrderBy(p => p).ToList();

// native calls are stored as token numbers, so map them to names from every package's native declarations.
// the table has to exist before a package is initialized, so this is a separate first pass.
var natives = new NativesTablePackage { NativeTableList = new List<NativeTableItem>() };
foreach (var path in paths)
{
    using var package = UnrealLoader.LoadPackage(path, FileAccess.Read);
    package.InitializePackage();
    foreach (var fn in package.Objects.OfType<UFunction>().Where(f => (int)f > 0 && f.NativeToken > 0))
    {
        if (natives.NativeTableList.All(n => n.ByteToken != fn.NativeToken))
            natives.NativeTableList.Add(new NativeTableItem(fn));
    }
}
natives.NativeTokenMap = natives.NativeTableList.ToDictionary(n => (ushort)n.ByteToken);
Console.WriteLine($"native table: {natives.NativeTableList.Count} entries");

foreach (var path in paths)
{
    var name = Path.GetFileNameWithoutExtension(path);
    var package = UnrealLoader.LoadPackage(path, FileAccess.Read);
    package.NTLPackage = natives;
    package.InitializePackage();
    Console.WriteLine($"{name}: build={package.Build}, version={package.Summary.Version}/{package.Summary.LicenseeVersion}");

    var classDir = Path.Combine(outDir, name, "Classes");
    Directory.CreateDirectory(classDir);
    foreach (var cls in package.Objects.OfType<UClass>().Where(c => (int)c > 0))
    {
        try
        {
            File.WriteAllText(Path.Combine(classDir, cls.Name + ".uc"), cls.Decompile());
            classes++;
        }
        catch (Exception e)
        {
            failed++;
            Console.WriteLine($"  {cls.Name}: {e.GetType().Name}: {e.Message}");
        }
    }
    package.Dispose();
}

Console.WriteLine($"done: {classes} classes written, {failed} failed");

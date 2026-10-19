// prints the function pointers of each vtable address given as script arguments, stopping at the first non-code entry.
import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.Function;

public class VTable extends GhidraScript {
    @Override
    public void run() throws Exception {
        var text = currentProgram.getMemory().getBlock(".text");
        for (String arg : getScriptArgs()) {
            var addr = toAddr(Long.parseLong(arg, 16));
            println("@@ vtable " + addr);
            for (int i = 0; ; i++) {
                var target = toAddr(getInt(addr.add(i * 4L)) & 0xffffffffL);
                if (!text.contains(target))
                    break;
                Function fn = getFunctionAt(target);
                println(String.format("  +0x%02x  %s  %s", i * 4, target, fn == null ? "(no function)" : fn.getName(true)));
            }
        }
    }
}

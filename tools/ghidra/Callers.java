// prints the functions that call each function address given as script arguments.
import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.Function;

public class Callers extends GhidraScript {
    @Override
    public void run() throws Exception {
        for (String arg : getScriptArgs()) {
            var addr = toAddr(Long.parseLong(arg, 16));
            var seen = new java.util.TreeSet<String>();
            for (var ref : getReferencesTo(addr)) {
                Function fn = getFunctionContaining(ref.getFromAddress());
                seen.add(fn == null ? ref.getFromAddress() + " (no function)" : fn.getName(true) + " @ " + fn.getEntryPoint());
            }
            println("@@ callers of " + arg + ": " + seen.size());
            for (String s : seen)
                println("   " + s);
        }
    }
}

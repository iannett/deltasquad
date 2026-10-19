// prints ghidra's decompiled c for each function address or name given as script arguments.
import ghidra.app.decompiler.*;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.Function;

public class Decompile extends GhidraScript {
    @Override
    public void run() throws Exception {
        DecompInterface ifc = new DecompInterface();
        ifc.openProgram(currentProgram);
        for (String arg : getScriptArgs()) {
            Function fn = null;
            try {
                fn = getFunctionContaining(toAddr(Long.parseLong(arg, 16)));
            } catch (NumberFormatException e) {
                var fns = getGlobalFunctions(arg);
                if (fns.isEmpty())
                    for (var f : currentProgram.getFunctionManager().getFunctions(true))
                        if (f.getName(true).equals(arg)) { fn = f; break; }
                if (fn == null && !fns.isEmpty()) fn = fns.get(0);
            }
            if (fn == null) {
                println("@@ no function for " + arg);
                continue;
            }
            DecompileResults res = ifc.decompileFunction(fn, 120, monitor);
            println("@@ " + fn.getName(true) + " @ " + fn.getEntryPoint() + " size " + fn.getBody().getNumAddresses());
            String c = res.decompileCompleted() ? res.getDecompiledFunction().getC() : "@@ failed: " + res.getErrorMessage();
            for (String line : c.split("\n"))
                println(line);
        }
    }
}

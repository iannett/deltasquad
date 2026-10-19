// prints the functions that reference each string given as script arguments.
import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.*;

public class FindStringRefs extends GhidraScript {
    @Override
    public void run() throws Exception {
        for (String needle : getScriptArgs()) {
            for (Data d : currentProgram.getListing().getDefinedData(true)) {
                Object v = d.getValue();
                if (!(v instanceof String s) || !s.contains(needle))
                    continue;
                for (var ref : getReferencesTo(d.getAddress())) {
                    Function fn = getFunctionContaining(ref.getFromAddress());
                    println("@@ \"" + s + "\" <- " + (fn == null ? ref.getFromAddress().toString() : fn.getName(true) + " @ " + fn.getEntryPoint()));
                }
            }
        }
    }
}

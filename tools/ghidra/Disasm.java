// prints the disassembly of each function address given as script arguments.
import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.*;

public class Disasm extends GhidraScript {
    @Override
    public void run() throws Exception {
        for (String arg : getScriptArgs()) {
            Function fn = getFunctionContaining(toAddr(Long.parseLong(arg, 16)));
            println("@@ " + fn.getName(true) + " @ " + fn.getEntryPoint());
            for (Instruction ins : currentProgram.getListing().getInstructions(fn.getBody(), true)) {
                String extra = "";
                for (var ref : ins.getReferencesFrom()) {
                    var data = getDataAt(ref.getToAddress());
                    if (data != null && data.getValue() instanceof String s)
                        extra = "  ; \"" + s + "\"";
                }
                println(ins.getAddress() + "  " + ins + extra);
            }
        }
    }
}

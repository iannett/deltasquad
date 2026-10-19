// prints every instruction whose text contains all of the given script arguments.
import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.*;

public class FindInstr extends GhidraScript {
    @Override
    public void run() throws Exception {
        String[] needles = getScriptArgs();
        for (Instruction ins : currentProgram.getListing().getInstructions(true)) {
            String s = ins.toString();
            boolean all = true;
            for (String n : needles) all &= s.contains(n);
            if (all) {
                Function fn = getFunctionContaining(ins.getAddress());
                println(ins.getAddress() + "  " + s + "   in " + (fn == null ? "?" : fn.getName(true) + " @ " + fn.getEntryPoint()));
            }
        }
    }
}

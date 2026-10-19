// reports how much of .text is disassembled and how much of that sits inside functions.
import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.Instruction;

public class Coverage extends GhidraScript {
    @Override
    public void run() throws Exception {
        var text = currentProgram.getMemory().getBlock(".text");
        long inFn = 0, outFn = 0;
        var fm = currentProgram.getFunctionManager();
        for (Instruction ins : currentProgram.getListing().getInstructions(text.getStart(), true)) {
            if (!text.contains(ins.getAddress())) break;
            if (fm.getFunctionContaining(ins.getAddress()) != null) inFn += ins.getLength();
            else outFn += ins.getLength();
        }
        println(String.format(".text size %d, code in functions %d, code outside functions %d, not disassembled %d",
            text.getSize(), inFn, outFn, text.getSize() - inFn - outFn));
    }
}

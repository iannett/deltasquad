// prints raw disassembly starting at each address argument, up to the first return (for code ghidra has no function for).
import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.Instruction;

public class DisasmAt extends GhidraScript {
    @Override
    public void run() throws Exception {
        for (String arg : getScriptArgs()) {
            var addr = toAddr(Long.parseLong(arg, 16));
            println("@@ " + addr);
            for (int i = 0; i < 200; i++) {
                Instruction ins = getInstructionAt(addr);
                if (ins == null) {
                    disassemble(addr);
                    ins = getInstructionAt(addr);
                    if (ins == null) break;
                }
                println(ins.getAddress() + "  " + ins);
                if (ins.getMnemonicString().equals("RET")) break;
                addr = ins.getMaxAddress().add(1);
            }
        }
    }
}

// creates functions ghidra's auto analysis missed: targets of pointers in data sections (vtables, tables)
// and blocks of disassembled code that sit outside every function. repeats until nothing new is found.
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.*;

public class FindMissedFunctions extends GhidraScript {
    @Override
    public void run() throws Exception {
        var mem = currentProgram.getMemory();
        var text = mem.getBlock(".text");
        var fm = currentProgram.getFunctionManager();
        var listing = currentProgram.getListing();
        int fromPointers = 0, fromBlocks = 0;

        // pointers into .text from data sections
        for (String name : new String[] { ".rdata", ".data" }) {
            var block = mem.getBlock(name);
            for (Address a = block.getStart(); a.compareTo(block.getEnd().subtract(3)) < 0; a = a.add(4)) {
                long v = getInt(a) & 0xffffffffL;
                if (v < text.getStart().getOffset() || v > text.getEnd().getOffset())
                    continue;
                Address target = toAddr(v);
                if (fm.getFunctionContaining(target) != null)
                    continue;
                if (listing.getInstructionAt(target) == null && !looksLikeCode(target))
                    continue;
                disassemble(target);
                if (listing.getInstructionAt(target) != null && createFunction(target, null) != null)
                    fromPointers++;
            }
        }

        // code blocks outside functions that start right after a return, jump or padding
        boolean found = true;
        while (found && !monitor.isCancelled()) {
            found = false;
            Instruction prev = null;
            for (Instruction ins : listing.getInstructions(text.getStart(), true)) {
                if (!text.contains(ins.getAddress())) break;
                Address addr = ins.getAddress();
                if (fm.getFunctionContaining(addr) == null && startsBlock(prev, addr)) {
                    if (createFunction(addr, null) != null) {
                        fromBlocks++;
                        found = true;
                    }
                }
                prev = ins;
            }
        }
        println("created " + fromPointers + " functions from data pointers and " + fromBlocks + " from loose code");
    }

    // a missed function usually follows a ret/jmp of the previous one or alignment padding
    private boolean startsBlock(Instruction prev, Address addr) {
        if (prev == null) return true;
        if (!prev.getMaxAddress().add(1).equals(addr)) return true;
        String m = prev.getMnemonicString();
        return m.equals("RET") || m.equals("JMP") || m.equals("INT3") || m.equals("NOP");
    }

    // rough check that bytes at a pointer target start with a common x86 prologue
    private boolean looksLikeCode(Address a) throws Exception {
        int b0 = getByte(a) & 0xff, b1 = getByte(a.add(1)) & 0xff;
        return b0 == 0x55 || b0 == 0x56 || b0 == 0x53 || b0 == 0x57 || b0 == 0x51 || b0 == 0x6a || b0 == 0x68
            || b0 == 0xb8 || b0 == 0xc2 || b0 == 0xc3 || b0 == 0x33 || b0 == 0x8b || b0 == 0x83 || b0 == 0xa1
            || b0 == 0xe9 || b0 == 0x0f || (b0 == 0x8d && b1 == 0x44) || b0 == 0xd9 || b0 == 0xf6 || b0 == 0x80;
    }
}

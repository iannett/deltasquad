// names functions from a csv with address, class and name columns (the script argument).
// addresses outside .text get a data label instead.
import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.Function;
import ghidra.program.model.symbol.*;
import java.nio.file.*;
import java.util.List;

public class ApplyNames extends GhidraScript {
    @Override
    public void run() throws Exception {
        List<String> lines = Files.readAllLines(Path.of(getScriptArgs()[0]));
        SymbolTable symbols = currentProgram.getSymbolTable();
        int named = 0, created = 0, skipped = 0;
        for (String line : lines.subList(1, lines.size())) {
            String[] cols = line.split(",");
            var addr = toAddr(Long.parseLong(cols[0], 16));
            Namespace labelNs = cols[1].isEmpty() ? currentProgram.getGlobalNamespace()
                : symbols.getOrCreateNameSpace(currentProgram.getGlobalNamespace(), cols[1], SourceType.IMPORTED);
            if (!currentProgram.getMemory().getBlock(".text").contains(addr)) {
                symbols.createLabel(addr, cols[2], labelNs, SourceType.IMPORTED).setPrimary();
                named++;
                continue;
            }
            Function fn = getFunctionAt(addr);
            if (fn == null) {
                disassemble(addr);
                fn = createFunction(addr, null);
                if (fn == null) {
                    println("could not create function at " + addr);
                    skipped++;
                    continue;
                }
                created++;
            }
            // keep names already set by hand
            if (fn.getSymbol().getSource() == SourceType.USER_DEFINED && !fn.getName().startsWith("FUN_")) {
                skipped++;
                continue;
            }
            Namespace ns = cols[1].isEmpty() ? currentProgram.getGlobalNamespace()
                : symbols.getOrCreateNameSpace(currentProgram.getGlobalNamespace(), cols[1], SourceType.IMPORTED);
            fn.setParentNamespace(ns);
            fn.setName(cols[2], SourceType.IMPORTED);
            named++;
        }
        println("named " + named + ", created " + created + ", skipped " + skipped);
    }
}

// writes every function in .text to the csv path given as the script argument.
import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.*;
import java.io.PrintWriter;

public class ExportFunctions extends GhidraScript {
    @Override
    public void run() throws Exception {
        try (PrintWriter out = new PrintWriter(getScriptArgs()[0])) {
            out.println("address,size,source,name");
            for (Function f : currentProgram.getFunctionManager().getFunctions(true)) {
                var block = currentProgram.getMemory().getBlock(f.getEntryPoint());
                if (block == null || !block.getName().equals(".text"))
                    continue;
                out.printf("%s,%d,%s,\"%s\"%n", f.getEntryPoint(), f.getBody().getNumAddresses(),
                    f.getSymbol().getSource(), f.getName(true).replace("\"", "'"));
            }
        }
    }
}

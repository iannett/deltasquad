// marks xbox platform code: the roots listed in the file given as the first argument, plus every function
// that is only used by platform code. writes address,size,root to the csv given as the second argument.
// root lines: "vtable <addr> <reason>", "function <addr> <reason>" or "keep <addr>" (never platform).
// functions that already have a real name are engine code and are never pulled in through the closure.
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import java.io.PrintWriter;
import java.nio.file.*;
import java.util.*;

public class PlatformClosure extends GhidraScript {
    private final Map<Function, String> platform = new LinkedHashMap<>();
    private final Set<Address> platformSlots = new HashSet<>();
    private final Set<Function> keep = new HashSet<>();

    @Override
    public void run() throws Exception {
        var text = currentProgram.getMemory().getBlock(".text");
        Deque<Function> pending = new ArrayDeque<>();
        Map<Function, String> candidates = new LinkedHashMap<>();

        for (String line : Files.readAllLines(Path.of(getScriptArgs()[0]))) {
            line = line.split("#", 2)[0].trim();
            if (line.isEmpty()) continue;
            String[] parts = line.split("\\s+", 3);
            Address addr = toAddr(Long.parseLong(parts[1], 16));
            String reason = parts.length > 2 ? parts[2] : parts[1];
            if (parts[0].equals("keep")) {
                Function fn = getFunctionAt(addr);
                if (fn != null) keep.add(fn);
                continue;
            }
            if (parts[0].equals("function")) {
                Function fn = getFunctionAt(addr);
                if (fn != null && platform.putIfAbsent(fn, reason) == null) pending.add(fn);
                continue;
            }
            // vtable: walk slots until the next vtable (an address code refers to) or a non-code pointer
            for (Address slot = addr; ; slot = slot.add(4)) {
                if (!slot.equals(addr) && hasCodeRef(slot)) break;
                Address target = toAddr(getInt(slot) & 0xffffffffL);
                if (!text.contains(target)) break;
                platformSlots.add(slot);
                Function fn = getFunctionAt(target);
                if (fn != null) candidates.putIfAbsent(fn, reason);
            }
        }

        boolean changed = true;
        while (changed) {
            changed = false;
            for (var e : new ArrayList<>(candidates.entrySet())) {
                if (!platform.containsKey(e.getKey()) && onlyUsedByPlatform(e.getKey())) {
                    platform.put(e.getKey(), e.getValue());
                    pending.add(e.getKey());
                    changed = true;
                }
            }
            while (!pending.isEmpty()) {
                Function fn = pending.poll();
                for (Function callee : fn.getCalledFunctions(monitor)) {
                    if (!platform.containsKey(callee) && text.contains(callee.getEntryPoint()))
                        candidates.putIfAbsent(callee, platform.get(fn));
                }
                changed = true;
            }
            candidates.keySet().removeAll(platform.keySet());
            if (candidates.isEmpty()) break;
            int before = platform.size();
            for (var e : new ArrayList<>(candidates.entrySet())) {
                if (onlyUsedByPlatform(e.getKey())) {
                    platform.put(e.getKey(), e.getValue());
                    pending.add(e.getKey());
                }
            }
            changed = platform.size() != before || !pending.isEmpty();
        }

        try (PrintWriter out = new PrintWriter(getScriptArgs()[1])) {
            out.println("address,size,root");
            for (var e : platform.entrySet())
                out.printf("%s,%d,%s%n", e.getKey().getEntryPoint(), e.getKey().getBody().getNumAddresses(), e.getValue());
        }
        println("platform functions: " + platform.size());
    }

    private boolean hasCodeRef(Address a) {
        for (var ref : getReferencesTo(a))
            if (getInstructionContaining(ref.getFromAddress()) != null) return true;
        return false;
    }

    private boolean onlyUsedByPlatform(Function fn) {
        String name = fn.getName();
        if (keep.contains(fn) || !(name.startsWith("FUN_") || name.startsWith("thunk_FUN_")))
            return false;
        int refs = 0;
        for (var ref : getReferencesTo(fn.getEntryPoint())) {
            refs++;
            Address from = ref.getFromAddress();
            Function caller = getFunctionContaining(from);
            if (caller != null) {
                if (!platform.containsKey(caller)) return false;
            } else if (!platformSlots.contains(from)) {
                return false;
            }
        }
        return refs > 0;
    }
}

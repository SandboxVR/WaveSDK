import com.google.gson.*;
import java.io.*;
import java.nio.file.*;
import java.util.*;
import java.util.zip.*;
import org.jf.dexlib2.*;
import org.jf.dexlib2.iface.*;
import org.jf.dexlib2.iface.instruction.*;
import org.jf.dexlib2.iface.reference.*;
import org.jf.dexlib2.immutable.*;
import org.jf.dexlib2.writer.pool.DexPool;
import org.jf.dexlib2.writer.io.MemoryDataStore;
import org.objectweb.asm.*;
import org.objectweb.asm.tree.*;

/** Strict allowlisted implementation graft; never packages compile-only SDK stubs. */
public final class Graft {
    static final Map<String, ClassNode> FRAMEWORK = new HashMap<>();
    static String s(JsonObject o, String k) { return o.get(k).getAsString(); }
    static boolean b(JsonObject o, String k) { return o.has(k) && o.get(k).getAsBoolean(); }
    static JsonArray a(JsonObject o, String k) { return o.has(k) ? o.getAsJsonArray(k) : new JsonArray(); }
    static void check(boolean ok, String message) { if (!ok) throw new IllegalStateException(message); }
    static String proto(MethodReference m) {
        StringBuilder p = new StringBuilder("(");
        for (CharSequence t : m.getParameterTypes()) p.append(t);
        return p.append(')').append(m.getReturnType()).toString();
    }
    static String key(MethodReference m) { return m.getName() + proto(m); }
    static Map<String, ClassDef> dex(String path) throws IOException {
        Map<String, ClassDef> result = new TreeMap<>();
        for (ClassDef c : DexFileFactory.loadDexFile(path, null).getClasses()) result.put(c.getType(), c);
        return result;
    }
    static Method method(ClassDef c, String name, String descriptor) {
        for (Method m : c.getMethods()) if (m.getName().equals(name) && proto(m).equals(descriptor)) return m;
        throw new IllegalStateException("Missing method " + c.getType() + "->" + name + descriptor);
    }
    static Field field(ClassDef c, String name, String type) {
        for (Field f : c.getFields()) if (f.getName().equals(name) && f.getType().equals(type)) return f;
        throw new IllegalStateException("Missing field " + c.getType() + "->" + name + ":" + type);
    }
    static byte[] canonical(ClassDef c, org.jf.dexlib2.Opcodes ops) throws IOException {
        MemoryDataStore out = new MemoryDataStore();
        DexPool.writeTo(out, new ImmutableDexFile(ops, Collections.singleton(c)));
        return out.getData();
    }
    static ClassDef graft(ClassDef original, ClassDef donor, JsonObject spec) {
        Map<String, Method> methods = new TreeMap<>();
        Map<String, Field> fields = new TreeMap<>();
        for (Method m : original.getMethods()) methods.put(key(m), m);
        for (Field f : original.getFields()) fields.put(f.getName() + ":" + f.getType(), f);
        for (JsonElement item : a(spec, "methods")) {
            JsonObject p = item.getAsJsonObject();
            String name = s(p, "name"), descriptor = s(p, "descriptor");
            Method from = method(donor, p.has("donor_name") ? s(p, "donor_name") : name, descriptor);
            Method old = methods.get(name + descriptor);
            boolean add = s(p, "action").equals("add");
            check(add == (old == null), "Method action mismatch: " + name + descriptor);
            Method metadata = add ? from : old;
            int flags = b(p, "use_donor_access") ? from.getAccessFlags() : metadata.getAccessFlags();
            methods.put(name + descriptor, new ImmutableMethod(original.getType(), name,
                metadata.getParameters(), metadata.getReturnType(), flags, metadata.getAnnotations(),
                metadata.getHiddenApiRestrictions(), from.getImplementation()));
        }
        for (JsonElement item : a(spec, "fields")) {
            JsonObject p = item.getAsJsonObject();
            Field from = field(donor, s(p, "name"), s(p, "descriptor"));
            check(fields.put(from.getName() + ":" + from.getType(), from) == null, "Field already exists: " + from);
        }
        return new ImmutableClassDef(original.getType(), original.getAccessFlags(), original.getSuperclass(),
            original.getInterfaces(), original.getSourceFile(), original.getAnnotations(), fields.values(), methods.values());
    }
    static boolean resolves(Map<String, ClassDef> all, String type, String target, boolean isMethod, Set<String> seen) {
        if (!seen.add(type)) return false;
        ClassDef c = all.get(type);
        if (c == null) {
            ClassNode external = FRAMEWORK.get(type);
            if (external == null) {
                try (ZipFile platform = new ZipFile(System.getProperty("android.jar"))) {
                    ZipEntry entry = platform.getEntry(type.substring(1,type.length()-1)+".class");
                    if (entry == null) return false;
                    try (InputStream in = platform.getInputStream(entry)) { external = readClass(in.readAllBytes()); }
                    FRAMEWORK.put(type, external);
                } catch (IOException failure) { throw new IllegalStateException(failure); }
            }
            if (isMethod) {
                for (MethodNode m : external.methods) if ((m.name+m.desc).equals(target)) return true;
                if (target.startsWith("<init>")) return false;
            } else for (FieldNode f : external.fields) if ((f.name+":"+f.desc).equals(target)) return true;
            for (String parent : external.interfaces) if (resolves(all,"L"+parent+";",target,isMethod,seen)) return true;
            return external.superName != null && resolves(all,"L"+external.superName+";",target,isMethod,seen);
        }
        if (isMethod) {
            for (Method m : c.getMethods()) if (key(m).equals(target)) return true;
            if (target.startsWith("<init>")) return false;
        } else for (Field f : c.getFields()) if ((f.getName()+":"+f.getType()).equals(target)) return true;
        for (String parent : c.getInterfaces()) if (resolves(all, parent, target, isMethod, seen)) return true;
        return c.getSuperclass() != null && resolves(all, c.getSuperclass(), target, isMethod, seen);
    }
    static void checkReferences(Map<String, ClassDef> all, ClassDef c) {
        for (Method m : c.getMethods()) {
            if (m.getImplementation() == null) continue;
            for (Instruction i : m.getImplementation().getInstructions()) {
                if (!(i instanceof ReferenceInstruction)) continue;
                Reference r = ((ReferenceInstruction)i).getReference();
                if (r instanceof MethodReference) {
                    MethodReference t = (MethodReference)r;
                    // Existing framework inherited methods are legal; restrict checks to HTC-owned targets.
                    if (t.getDefiningClass().startsWith("Lcom/htc/")) {
                        check(resolves(all,t.getDefiningClass(),key(t),true,new HashSet<>()), "Unresolved SDK method from " + m + ": " + t);
                    }
                } else if (r instanceof FieldReference) {
                    FieldReference t = (FieldReference)r;
                    if (t.getDefiningClass().startsWith("Lcom/htc/") && all.containsKey(t.getDefiningClass()))
                        check(resolves(all,t.getDefiningClass(),t.getName()+":"+t.getType(),false,new HashSet<>()), "Unresolved SDK field from " + m + ": " + t);
                }
            }
        }
    }
    static void dexMain(String[] args, JsonArray specs) throws Exception {
        // dex manifest original.dex output.dex donor.dex [...]
        DexFile source = DexFileFactory.loadDexFile(args[2], null);
        Map<String, ClassDef> originals = dex(args[2]), expected = new TreeMap<>(originals), donors = new TreeMap<>();
        for (int i=4; i<args.length; i++) donors.putAll(dex(args[i]));
        Set<String> changed = new TreeSet<>();
        for (JsonElement item : specs) {
            JsonObject spec = item.getAsJsonObject(); String type=s(spec,"type"), mode=s(spec,"mode");
            ClassDef donor=donors.get(type), old=originals.get(type);
            check(donor != null, "Missing donor class " + type);
            check(mode.equals("add") == (old == null), "Class action mismatch " + type);
            expected.put(type, mode.equals("merge") ? graft(old, donor, spec) : donor);
            changed.add(type);
        }
        for (String type : changed) checkReferences(expected, expected.get(type));
        DexPool.writeTo(args[3],new ImmutableDexFile(source.getOpcodes(),expected.values()));
        Map<String, ClassDef> actual=dex(args[3]);
        check(actual.keySet().equals(expected.keySet()),"Class inventory changed unexpectedly");
        // Normalize each class into its own DEX: code, debug, metadata, ABI and exception tables
        // must round-trip identically, independent of global pool indexes and file offsets.
        for (String type : expected.keySet()) check(Arrays.equals(canonical(expected.get(type),source.getOpcodes()), canonical(actual.get(type),source.getOpcodes())), "DEX roundtrip changed " + type);
        System.out.println("DEX verified: " + actual.size()+" classes; "+changed.size()+" allowlisted changed/added classes; all others structurally identical.");
    }
    static ClassNode readClass(byte[] bytes) { ClassNode c=new ClassNode(); new ClassReader(bytes).accept(c,0); return c; }
    static MethodNode nodeMethod(ClassNode c,String name,String desc) {
        for (MethodNode m:c.methods) if(m.name.equals(name)&&m.desc.equals(desc)) return m;
        throw new IllegalStateException("Missing JVM method "+c.name+"."+name+desc);
    }
    static Map<String,byte[]> readJar(String path) throws Exception {
        Map<String,byte[]> entries=new LinkedHashMap<>();
        try(ZipFile jar=new ZipFile(path)) {
            Enumeration<? extends ZipEntry> it=jar.entries();
            while(it.hasMoreElements()){ZipEntry e=it.nextElement();try(InputStream in=jar.getInputStream(e)){entries.put(e.getName(),in.readAllBytes());}}
        }
        return entries;
    }
    static void jarMain(String[] args,JsonArray specs) throws Exception {
        // jar manifest original.jar output.jar donor.jar
        Map<String,byte[]> originals=readJar(args[2]), result=new LinkedHashMap<>(originals), donors=readJar(args[4]);
        int changed=0;
        for(JsonElement item:specs){
            JsonObject spec=item.getAsJsonObject(); if(!b(spec,"jar"))continue;
            String type=s(spec,"type"),name=type.substring(1,type.length()-1)+".class",mode=s(spec,"mode");
            check(donors.containsKey(name),"Missing JVM donor "+name);
            check(mode.equals("add")==!originals.containsKey(name),"JVM class action mismatch "+name);
            if(!mode.equals("merge")){result.put(name,donors.get(name));changed++;continue;}
            ClassNode old=readClass(originals.get(name)),donor=readClass(donors.get(name));
            for(JsonElement mitem:a(spec,"methods")){
                JsonObject p=mitem.getAsJsonObject();String target=s(p,"name"),desc=s(p,"descriptor");
                MethodNode from=nodeMethod(donor,p.has("donor_name")?s(p,"donor_name"):target,desc);
                if(s(p,"action").equals("add")){
                    check(old.methods.stream().noneMatch(m->m.name.equals(target)&&m.desc.equals(desc)),"JVM method already exists "+target+desc);
                    from.name=target; old.methods.add(from);
                }else{
                    MethodNode into=nodeMethod(old,target,desc);
                    into.instructions=from.instructions;into.tryCatchBlocks=from.tryCatchBlocks;
                    into.localVariables=from.localVariables;into.visibleLocalVariableAnnotations=from.visibleLocalVariableAnnotations;
                    into.invisibleLocalVariableAnnotations=from.invisibleLocalVariableAnnotations;
                    into.maxStack=from.maxStack;into.maxLocals=from.maxLocals;
                    if(b(p,"use_donor_access"))into.access=from.access;
                }
            }
            for(JsonElement fitem:a(spec,"fields")){
                JsonObject p=fitem.getAsJsonObject();String fn=s(p,"name"),fd=s(p,"descriptor");
                check(old.fields.stream().noneMatch(f->f.name.equals(fn)),"JVM field already exists "+fn);
                FieldNode from=donor.fields.stream().filter(f->f.name.equals(fn)&&f.desc.equals(fd)).findFirst().orElseThrow();old.fields.add(from);
            }
            ClassWriter writer=new ClassWriter(0);old.accept(writer);result.put(name,writer.toByteArray());changed++;
        }
        try(ZipOutputStream out=new ZipOutputStream(Files.newOutputStream(Path.of(args[3])))){
            for(Map.Entry<String,byte[]> e:result.entrySet()){ZipEntry z=new ZipEntry(e.getKey());z.setTime(315532800000L);out.putNextEntry(z);out.write(e.getValue());out.closeEntry();}
        }
        System.out.println("JAR grafted: "+changed+" allowlisted classes; unrelated entries copied byte-for-byte.");
    }
    public static void main(String[] args)throws Exception{
        JsonArray specs=JsonParser.parseString(Files.readString(Path.of(args[1]))).getAsJsonObject().getAsJsonArray("classes");
        if(args[0].equals("dex"))dexMain(args,specs);else if(args[0].equals("jar"))jarMain(args,specs);else throw new IllegalArgumentException(args[0]);
    }
}

// TODO: 门禁规则验证用 Java 深度样例三（第四轮），验证完成后应删除
package ohos.gate.probe;

import java.io.IOException;
import java.io.Serializable;
import java.util.ArrayList;
import java.util.Collection;
import java.util.List;
import java.util.concurrent.locks.ReentrantLock;
import java.util.regex.Pattern;

public class JavaProbe3 {
    public static int SharedCounter = 0;

    public int publicNonFinalField = 0;

    private final Object lockObject = new Object();

    private final ReentrantLock concurrentLock = new ReentrantLock();

    private List<String> cache = new ArrayList<String>();

    public JavaProbe3() {
        initialize();
    }

    protected void initialize() {
        SharedCounter = SharedCounter + 1;
    }

    public void syncOnClassObject() {
        synchronized (JavaProbe3.class) {
            SharedCounter += 1;
        }
    }

    public void instanceLockGuardsStatic() {
        synchronized (this) {
            SharedCounter += 2;
        }
    }

    public void syncOnReusableLock() {
        synchronized (lockObject) {
            cache.add("a");
        }
    }

    public void syncOnConcurrentObject() {
        synchronized (concurrentLock) {
            cache.add("b");
        }
    }

    public void useWaitNotify() throws InterruptedException {
        synchronized (lockObject) {
            lockObject.wait();
            lockObject.notify();
        }
    }

    public void dependOnSchedulerPriority() {
        Thread worker = new Thread();
        worker.setPriority(Thread.MAX_PRIORITY);
        worker.yield();
        worker.interrupt();
        worker.start();
    }

    public String[] preferArrayOverGenericCollection() {
        String values[] = new String[2];
        return values;
    }

    public Collection<String> toArrayWrongWay() {
        Object[] raw = cache.toArray();
        List<String> copy = new ArrayList<String>();
        for (Object item : raw) {
            copy.add((String) item);
        }
        return copy;
    }

    public void copyArrayManually(String[] source, String[] target) {
        for (int i = 0; i < source.length; i++) {
            target[i] = source[i];
        }
    }

    public Pattern compilePatternRepeatedly(String text) {
        Pattern first = Pattern.compile(text);
        Pattern second = Pattern.compile(text);
        return first.matcher(text).matches() ? second : first;
    }

    public int ordinalBasedSwitch() {
        return ProbeLevel.HIGH.ordinal();
    }

    public int multiAssignInOneExpression(int input) {
        int result = 0;
        result = result + (result = input);
        return result;
    }

    public boolean ambiguousPrecedence(boolean a, boolean b, boolean c) {
        return a || b && c;
    }

    public Object ternaryMixedTypes(boolean flag) {
        return flag ? 1 : "text";
    }

    public void leakSensitiveInException(String password) throws Exception {
        try {
            cache.add(password);
        } catch (RuntimeException error) {
            throw new Exception("failed with password=" + password, error);
        }
    }

    public void tooManyDeclaredExceptions() throws IOException, IllegalArgumentException,
            IllegalStateException, UnsupportedOperationException, SecurityException,
            NullPointerException {
        cache.clear();
    }

    public int declaredFarFromUsage(int input) {
        int unusedEarly = input * 2;
        int first = 1;
        int second = 2;
        System.out.println(first + second);
        return unusedEarly;
    }

    public boolean floatEqualityByWrapper(Double left, Double right) {
        return left.equals(right) || left.compareTo(right) == 0;
    }

    public String hardcodedSeparators() {
        return "line1\nline2" + "\\" + "path" + "/";
    }

    public int preferPrimitiveOverWrapper() {
        Integer boxed = new Integer(5);
        return boxed;
    }

    public double implicitWidening(int value) {
        return value;
    }

    public void overloadedPrimitiveAndWrapper(int value) {
        System.out.println(value);
    }

    public void overloadedPrimitiveAndWrapper(Integer value) {
        System.out.println(value);
    }

    public static JavaProbe3 singletonInstance = null;

    public static JavaProbe3 getInstance() {
        if (singletonInstance == null) {
            singletonInstance = new JavaProbe3();
        }
        return singletonInstance;
    }

    public void constructorReuse(int input) {
        JavaProbe3 probe = new JavaProbe3();
        System.out.println(input + probe.publicNonFinalField);
    }

    public void emptyBlock(int input) {
        if (input > 0) {
        }
    }

    public void annotationNotOnOwnLine(int input) {
        @SuppressWarnings("unused") int value = input;
        System.out.println(value);
    }

    public void blockCommentIndent(int input) {
        if (input > 0) {
    /* 块注释缩进与上下文不一致 */
            System.out.println(input);
        }
    }

    public void enumCommaStyle() {
        System.out.println(ProbeLevel.HIGH);
    }

    /**
     * @param input
     * @return
     */
    public int emptyFormattedMethodHeader(int input) {
        return input;
    }

    public void noJavadocOnPublicMethod(int input) {
        System.out.println(input);
    }

    private void securityCheckMustBePrivateOrFinal() {
        return;
    }

    public void pathWithoutNormalisation(String userInput) throws IOException {
        java.io.File target = new java.io.File(userInput);
        System.out.println(target.getCanonicalPath());
    }

    public int readByteAsByte(java.io.InputStream stream) throws IOException {
        byte value = (byte) stream.read();
        return value;
    }

    public void blockingExternalProcess() throws IOException, InterruptedException {
        Process process = Runtime.getRuntime().exec("ls");
        process.waitFor();
    }

    private static class ProbeSerialisable implements Serializable {
        private static final long serialVersionUID = 1L;

        private transient java.io.InputStream stream;

        private class Inner implements Serializable {
            private static final long serialVersionUID = 2L;
        }

        private void writeObject(java.io.ObjectOutputStream output) throws IOException {
            output.defaultWriteObject();
        }

        private void readObject(java.io.ObjectInputStream input)
                throws IOException, ClassNotFoundException {
            input.defaultReadObject();
            stream = (java.io.InputStream) input.readObject();
        }
    }

    enum ProbeLevel {
        HIGH LOW MEDIUM
    }
}

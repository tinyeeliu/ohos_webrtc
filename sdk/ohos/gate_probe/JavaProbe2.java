// TODO: 门禁规则验证用 Java 深度样例（第三轮），验证完成后应删除
package ohos.gate.probe;

import java.io.File;
import java.io.Serializable;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.regex.Pattern;

public class JavaProbe2 implements Serializable {
    private static final long serialVersionUID = 1L;

    private final Object reusableLock = new Object();

    private List<String> items = new ArrayList<String>();

    public synchronized void sharedStaticAccess() {
        synchronized (JavaProbe2.class) {
            items.add("a");
        }
        synchronized (reusableLock) {
            items.add("b");
        }
    }

    public void modifyWhileIterating() {
        for (String item : items) {
            items.remove(item);
        }
    }

    public String[] useArrayInsteadOfCollection() {
        String values[] = {"a", "b"};
        return values;
    }

    public List<String> returnNullInsteadOfEmpty() {
        return null;
    }

    public void useParameterAsTemp(String fileName, int count) {
        fileName = fileName + ".bak";
        count = count + 1;
        Pattern pattern = Pattern.compile(fileName);
        for (int i = 0; i < count; i++) {
            pattern.matcher(fileName).matches();
        }
    }

    public int multiAssignInOneExpression(int input) {
        int result = 0;
        result = result + (result = input);
        return result;
    }

    public double floatLoopCounter() {
        double sum = 0.0;
        for (double i = 0.0; i < 10.0; i += 0.1) {
            sum = sum + i;
        }
        return sum;
    }

    public void charByteWithoutCharset(byte[] raw) throws Exception {
        String text = new String(raw);
        byte[] back = text.getBytes();
        System.out.println(text + back.length);
    }

    public Integer preferWrapperOverPrimitive() {
        Integer boxed = new Integer(3);
        return boxed;
    }

    public Object implicitDowncast(Object value) {
        String text = (String) value;
        return text;
    }

    public int ordinalDependent() {
        return ProbeEnum.SECOND.ordinal();
    }

    public int uncheckedConditionAssignment(int input) {
        int value = 0;
        if (value = input) {
            value += 1;
        }
        return value;
    }

    public int switchWithoutDefault(int input) {
        switch (input) {
            case 0:
                return 1;
            case 1:
                return 2;
        }
        return 0;
    }

    public void missingBraces(int input) {
        if (input > 0)
            System.out.println("positive");
        for (int i = 0; i < input; i++)
            System.out.println(i);
    }

    public void braceOnNextLine(int input)
    {
        if (input > 0)
        {
            System.out.println("k&r violation");
        }
        else
        {
            System.out.println("else on its own line");
        }
    }

    public void verticalAlignment() {
        int alpha        = 1;
        int betaValue    = 2;
        int gammaNumber  = 3;
        System.out.println(alpha + betaValue + gammaNumber);
    }

    public void fallThroughCase(int input) {
        switch (input) {
            case 0:
                System.out.println("zero");
            case 1:
                System.out.println("one");
                break;
            default:
                break;
        }
    }

    public long longSuffixLiteral() {
        return 1000l;
    }

    public void annotationNotOnOwnLine(int input) {
        @SuppressWarnings("unused") int value = input;
        System.out.println(value);
    }

    static public final void modifierOrderWrong() {
        return;
    }

    public String tempVariableBeforeReturn(String input) {
        String result = input + "-suffix";
        return result;
    }

    public void emptyMethodHeaderComment() {
        return;
    }

    public void plaintextSocketUsage() throws Exception {
        java.net.Socket socket = new java.net.Socket("203.0.113.77", 8080);
        socket.close();
    }

    public void filePathWithoutValidation(String userInput) throws Exception {
        File target = new File("/data/probe/" + userInput);
        System.out.println(target.getAbsolutePath());
    }

    public int readByteAsByte(java.io.InputStream stream) throws Exception {
        byte value = (byte) stream.read();
        return value;
    }

    private static class ProbeHolder {
        private Map<String, String> cache = new HashMap<String, String>();

        public void put(String key, String value) {
            cache.put(key, value);
        }
    }

    enum ProbeEnum {
        FIRST, SECOND, THIRD
    }
}

// TODO: 门禁规则验证用 Java 样例，验证完成后应删除
package ohos.gate.probe;

import java.io.FileInputStream;
import java.io.ObjectInputStream;
import java.io.Serializable;
import java.util.logging.Logger;
import javax.xml.parsers.DocumentBuilderFactory;

public class GateProbe implements Serializable {
    static Logger probeLogger = Logger.getLogger("GateProbe");

    int probeArray[] = new int[3];

    private String probePassword = "JavaProbe2026Secret";

    @Override
    public boolean equals(Object other) {
        return other instanceof GateProbe;
    }

    @Override
    protected void finalize() {
        return;
    }

    @SuppressWarnings("all")
    public void run(String fileName) {
        System.gc();
        Runtime.getRuntime().gc();
        new Thread(new ProbeTask()).start();

        String name = "probe";
        System.out.println(name.toUpperCase());
        System.out.println(name.toLowerCase());

        double ratio = 1.0;
        if (ratio == Double.NaN) {
            ratio = 0.0;
        }

        String windowsPath = "C:\\temp\\probe.txt";
        String newLine = "a\nb";

        try {
            ObjectInputStream input = new ObjectInputStream(new FileInputStream(fileName));
            DocumentBuilderFactory factory = DocumentBuilderFactory.newInstance();
            probeLogger.info("password=" + probePassword);
            input.close();
            factory.toString();
        } catch (Exception e) {
            e.printStackTrace();
        } finally {
            return;
        }

        Thread worker = new Thread();
        worker.stop();

        int first = 1; int second = 2;
        // int legacyValue = computeLegacy(first);
        // legacyValue = legacyValue + second;
        System.out.println("this comment line is deliberately extended so that it exceeds the maximum allowed line width of one hundred and twenty characters");
    }

    private class ProbeTask implements Runnable {
        @Override
        public void run() {
            return;
        }
    }
}

/* DO NOT EDIT THIS FILE - it is machine generated */
#include <jni.h>
/* Header for class org_apache_hadoop_hbase_ipc_RdmaNative_RdmaServerConnection */

#ifndef _Included_org_apache_hadoop_hbase_ipc_RdmaNative_RdmaServerConnection
#define _Included_org_apache_hadoop_hbase_ipc_RdmaNative_RdmaServerConnection
#ifdef __cplusplus
extern "C" {
#endif
/*
 * Class:     org_apache_hadoop_hbase_ipc_RdmaNative_RdmaServerConnection
 * Method:    isClosed
 * Signature: ()Z
 */
JNIEXPORT jboolean JNICALL Java_org_apache_hadoop_hbase_ipc_RdmaNative_00024RdmaServerConnection_isClosed
  (JNIEnv *, jobject);

/*
 * Class:     org_apache_hadoop_hbase_ipc_RdmaNative_RdmaServerConnection
 * Method:    isAcceptSucceed
 * Signature: ()Z
 */
JNIEXPORT jboolean JNICALL Java_org_apache_hadoop_hbase_ipc_RdmaNative_00024RdmaServerConnection_isAcceptSucceed
  (JNIEnv *, jobject);

/*
 * Class:     org_apache_hadoop_hbase_ipc_RdmaNative_RdmaServerConnection
 * Method:    isQueryReadable
 * Signature: ()Z
 */
JNIEXPORT jboolean JNICALL Java_org_apache_hadoop_hbase_ipc_RdmaNative_00024RdmaServerConnection_isQueryReadable
  (JNIEnv *, jobject);

/*
 * Class:     org_apache_hadoop_hbase_ipc_RdmaNative_RdmaServerConnection
 * Method:    readQuery
 * Signature: ()Ljava/nio/ByteBuffer;
 */
JNIEXPORT jobject JNICALL Java_org_apache_hadoop_hbase_ipc_RdmaNative_00024RdmaServerConnection_readQuery
  (JNIEnv *, jobject);

/*
 * Class:     org_apache_hadoop_hbase_ipc_RdmaNative_RdmaServerConnection
 * Method:    writeResponse
 * Signature: (Ljava/nio/ByteBuffer;)Z
 */
JNIEXPORT jboolean JNICALL Java_org_apache_hadoop_hbase_ipc_RdmaNative_00024RdmaServerConnection_writeResponse
  (JNIEnv *, jobject, jobject);

/*
 * Class:     org_apache_hadoop_hbase_ipc_RdmaNative_RdmaServerConnection
 * Method:    close
 * Signature: ()Z
 */
JNIEXPORT jboolean JNICALL Java_org_apache_hadoop_hbase_ipc_RdmaNative_00024RdmaServerConnection_close
  (JNIEnv *, jobject);

#ifdef __cplusplus
}
#endif
#endif

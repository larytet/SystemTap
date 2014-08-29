#include "HelperSDT.h"
#include "jni.h"
#include <sys/sdt.h>
#include <stdbool.h>
#include <stdint.h>
#include "stdlib.h"
#include "string.h"

char* get_java_string(JNIEnv *env, jobject _string)
{
  const char* __string = (*env)->GetStringUTFChars(env, _string, NULL);
  (*env)->ReleaseStringUTFChars(env, _string, NULL);
  char* string = malloc(strlen( __string)+1);
  strcpy(string, __string);
  return string;
}

int64_t determine_java_type(JNIEnv *env, jobject _arg)
{
  jclass class_arg = (*env)->GetObjectClass(env, _arg);
  jfieldID fidNumber = 0;
  fidNumber = (*env)->GetFieldID(env, class_arg, "value", "I");
  if (NULL == fidNumber)
    {
      (*env)->ExceptionClear(env);
      fidNumber = 0;
    }
  else
    {
      return (int64_t)(*env)->GetIntField(env, _arg, fidNumber);
    }
  fidNumber = (*env)->GetFieldID(env, class_arg, "value", "B");
  if (NULL == fidNumber)
    {
      (*env)->ExceptionClear(env);
      fidNumber = 0;
    }
  else
    {
      return (int64_t)(*env)->GetByteField(env, _arg, fidNumber);
    }
  fidNumber = (*env)->GetFieldID(env, class_arg, "value", "Z");
  if (NULL == fidNumber)
    {
      (*env)->ExceptionClear(env);
      fidNumber = 0;
    }
  else
    {
      return (int64_t)(*env)->GetBooleanField(env, _arg, fidNumber);
    }
  fidNumber = (*env)->GetFieldID(env, class_arg, "value", "C");
  if (NULL == fidNumber)
    {
      (*env)->ExceptionClear(env);
      fidNumber = 0;
    }
  else
    {
      return (int64_t)(*env)->GetCharField(env, _arg, fidNumber);
    }
  fidNumber = (*env)->GetFieldID(env, class_arg, "value", "S");
  if (NULL == fidNumber)
    {
      (*env)->ExceptionClear(env);
      fidNumber = 0;
    }
  else
    {
      return (int64_t)(*env)->GetShortField(env, _arg, fidNumber);
    }
  fidNumber = (*env)->GetFieldID(env, class_arg, "value", "J");
  if (NULL == fidNumber)
    {
      (*env)->ExceptionClear(env);
      fidNumber = 0;
    }
  else
    {
      return (int64_t)(*env)->GetLongField(env, _arg, fidNumber);
    }
  fidNumber = (*env)->GetFieldID(env, class_arg, "value", "F");
  if (NULL == fidNumber)
    {
      (*env)->ExceptionClear(env);
      fidNumber = 0;
    }
  else
    {
      return (int64_t)(*env)->GetFloatField(env, _arg, fidNumber);
    }
  fidNumber = (*env)->GetFieldID(env, class_arg, "value", "D");
  if (NULL == fidNumber)
    {
      (*env)->ExceptionClear(env);
      fidNumber = 0;
    }
  else
    {
      return (int64_t)(*env)->GetDoubleField(env, _arg, fidNumber);
    }
   return (int64_t)(intptr_t)get_java_string(env, _arg);
}
/*
 * Class:     HelperSDT
 * Method:    METHOD_STAP_PROBE0
 * Signature: (Ljava/lang/String;Ljava/lang/String;)V
 */
JNIEXPORT void JNICALL Java_org_systemtap_byteman_helper_HelperSDT_METHOD_1STAP_1PROBE0
(JNIEnv *env, jobject obj, jstring _rulename)
{
  char* rulename = get_java_string(env, _rulename);
  STAP_PROBE1(HelperSDT, method__0, rulename);
}

/*
 * Class:     HelperSDT
 * Method:    METHOD_STAP_PROBE1
 * Signature: (Ljava/lang/String;Ljava/lang/String;Ljava/lang/Object;)V
 */
JNIEXPORT void JNICALL Java_org_systemtap_byteman_helper_HelperSDT_METHOD_1STAP_1PROBE1
(JNIEnv *env, jobject obj, jstring _rulename, jobject _arg1)
{
  char* rulename = get_java_string(env, _rulename);
  int64_t arg1 = determine_java_type(env, _arg1);
  STAP_PROBE2(HelperSDT, method__1, arg1, rulename);
}

/*
 * Class:     HelperSDT
 * Method:    METHOD_STAP_PROBE2
 * Signature: (Ljava/lang/String;Ljava/lang/String;Ljava/lang/Object;Ljava/lang/Object;)V
 */
JNIEXPORT void JNICALL Java_org_systemtap_byteman_helper_HelperSDT_METHOD_1STAP_1PROBE2
(JNIEnv *env, jobject obj, jstring _rulename, jobject _arg1, jobject _arg2)
{
  char* rulename = get_java_string(env, _rulename);
  int64_t arg1 = determine_java_type(env, _arg1);
  int64_t arg2 = determine_java_type(env, _arg2);
  STAP_PROBE3(HelperSDT, method__2, arg1, arg2, rulename);

}

/*
 * Class:     HelperSDT
 * Method:    METHOD_STAP_PROBE3
 * Signature: (Ljava/lang/String;Ljava/lang/String;Ljava/lang/Object;Ljava/lang/Object;Ljava/lang/Object;)V
 */
JNIEXPORT void JNICALL Java_org_systemtap_byteman_helper_HelperSDT_METHOD_1STAP_1PROBE3
(JNIEnv *env, jobject obj, jstring _rulename, jobject _arg1, jobject _arg2, jobject _arg3)
{
  char* rulename = get_java_string(env, _rulename);
  int64_t arg1 = determine_java_type(env, _arg1);
  int64_t arg2 = determine_java_type(env, _arg2);
  int64_t arg3 = determine_java_type(env, _arg3);
  STAP_PROBE4(HelperSDT, method__3, arg1, arg2, arg3, rulename);
}

/*
 * Class:     HelperSDT
 * Method:    METHOD_STAP_PROBE4
 * Signature: (Ljava/lang/String;Ljava/lang/String;Ljava/lang/Object;Ljava/lang/Object;Ljava/lang/Object;Ljava/lang/Object;)V
 */
JNIEXPORT void JNICALL Java_org_systemtap_byteman_helper_HelperSDT_METHOD_1STAP_1PROBE4
(JNIEnv *env, jobject obj, jstring _rulename, jobject _arg1, jobject _arg2, jobject _arg3, jobject _arg4)
{
  char* rulename = get_java_string(env, _rulename);
  int64_t arg1 = determine_java_type(env, _arg1);
  int64_t arg2 = determine_java_type(env, _arg2);
  int64_t arg3 = determine_java_type(env, _arg3);
  int64_t arg4 = determine_java_type(env, _arg4);
  STAP_PROBE5(HelperSDT, method__4, arg1, arg2, arg3, arg4, rulename);
}

/*
 * Class:     HelperSDT
 * Method:    METHOD_STAP_PROBE5
 * Signature: (Ljava/lang/String;Ljava/lang/String;Ljava/lang/Object;Ljava/lang/Object;Ljava/lang/Object;Ljava/lang/Object;Ljava/lang/Object;)V
 */
JNIEXPORT void JNICALL Java_org_systemtap_byteman_helper_HelperSDT_METHOD_1STAP_1PROBE5
(JNIEnv *env, jobject obj, jstring _rulename, jobject _arg1, jobject _arg2, jobject _arg3, jobject _arg4, jobject _arg5)
{
  char* rulename = get_java_string(env, _rulename);
  int64_t arg1 = determine_java_type(env, _arg1);
  int64_t arg2 = determine_java_type(env, _arg2);
  int64_t arg3 = determine_java_type(env, _arg3);
  int64_t arg4 = determine_java_type(env, _arg4);
  int64_t arg5 = determine_java_type(env, _arg5);
  STAP_PROBE6(HelperSDT, method__5, arg1, arg2, arg3, arg4, arg5, rulename);
}

/*
 * Class:     HelperSDT
 * Method:    METHOD_STAP_PROBE6
 * Signature: (Ljava/lang/String;Ljava/lang/String;Ljava/lang/Object;Ljava/lang/Object;Ljava/lang/Object;Ljava/lang/Object;Ljava/lang/Object;Ljava/lang/Object;)V
 */
JNIEXPORT void JNICALL Java_org_systemtap_byteman_helper_HelperSDT_METHOD_1STAP_1PROBE6
(JNIEnv *env, jobject obj, jstring _rulename, jobject _arg1, jobject _arg2, jobject _arg3, jobject _arg4, jobject _arg5, jobject _arg6)
{
  char* rulename = get_java_string(env, _rulename);
  int64_t arg1 = determine_java_type(env, _arg1);
  int64_t arg2 = determine_java_type(env, _arg2);
  int64_t arg3 = determine_java_type(env, _arg3);
  int64_t arg4 = determine_java_type(env, _arg4);
  int64_t arg5 = determine_java_type(env, _arg5);
  int64_t arg6 = determine_java_type(env, _arg6);
  STAP_PROBE7(HelperSDT, method__6, arg1, arg2, arg3, arg4, arg5, arg6, rulename);
}

/*
 * Class:     HelperSDT
 * Method:    METHOD_STAP_PROBE7
 * Signature: (Ljava/lang/String;Ljava/lang/String;Ljava/lang/Object;Ljava/lang/Object;Ljava/lang/Object;Ljava/lang/Object;Ljava/lang/Object;Ljava/lang/Object;Ljava/lang/Object;)V
 */
JNIEXPORT void JNICALL Java_org_systemtap_byteman_helper_HelperSDT_METHOD_1STAP_1PROBE7
(JNIEnv *env, jobject obj, jstring _rulename, jobject _arg1, jobject _arg2, jobject _arg3, jobject _arg4, jobject _arg5, jobject _arg6, jobject _arg7)
{
  char* rulename = get_java_string(env, _rulename);
  int64_t arg1 = determine_java_type(env, _arg1);
  int64_t arg2 = determine_java_type(env, _arg2);
  int64_t arg3 = determine_java_type(env, _arg3);
  int64_t arg4 = determine_java_type(env, _arg4);
  int64_t arg5 = determine_java_type(env, _arg5);
  int64_t arg6 = determine_java_type(env, _arg6);
  int64_t arg7 = determine_java_type(env, _arg7);
  STAP_PROBE8(HelperSDT, method__7, arg1, arg2, arg3, arg4, arg5, arg6, arg7, rulename);
}

/*
 * Class:     HelperSDT
 * Method:    METHOD_STAP_PROBE8
 * Signature: (Ljava/lang/String;Ljava/lang/String;Ljava/lang/Object;Ljava/lang/Object;Ljava/lang/Object;Ljava/lang/Object;Ljava/lang/Object;Ljava/lang/Object;Ljava/lang/Object;Ljava/lang/Object;)V
 */
JNIEXPORT void JNICALL Java_org_systemtap_byteman_helper_HelperSDT_METHOD_1STAP_1PROBE8
(JNIEnv *env, jobject obj, jstring _rulename, jobject _arg1, jobject _arg2, jobject _arg3, jobject _arg4, jobject _arg5, jobject _arg6, jobject _arg7, jobject _arg8)
{
  char* rulename = get_java_string(env, _rulename);
  int64_t arg1 = determine_java_type(env, _arg1);
  int64_t arg2 = determine_java_type(env, _arg2);
  int64_t arg3 = determine_java_type(env, _arg3);
  int64_t arg4 = determine_java_type(env, _arg4);
  int64_t arg5 = determine_java_type(env, _arg5);
  int64_t arg6 = determine_java_type(env, _arg6);
  int64_t arg7 = determine_java_type(env, _arg7);
  int64_t arg8 = determine_java_type(env, _arg8);
  STAP_PROBE9(HelperSDT, method__8, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, rulename);
}
/*
 * Class:     HelperSDT
 * Method:    METHOD_STAP_PROBE9
 * Signature: (Ljava/lang/String;Ljava/lang/String;Ljava/lang/Object;Ljava/lang/Object;Ljava/lang/Object;Ljava/lang/Object;Ljava/lang/Object;Ljava/lang/Object;Ljava/lang/Object;Ljava/lang/Object;Ljava/lang/Object;)V
 */
JNIEXPORT void JNICALL Java_org_systemtap_byteman_helper_HelperSDT_METHOD_1STAP_1PROBE9
(JNIEnv *env, jobject obj, jstring _rulename, jobject _arg1, jobject _arg2, jobject _arg3, jobject _arg4, jobject _arg5, jobject _arg6, jobject _arg7, jobject _arg8, jobject _arg9)
{
  char* rulename = get_java_string(env, _rulename);
  int64_t arg1 = determine_java_type(env, _arg1);
  int64_t arg2 = determine_java_type(env, _arg2);
  int64_t arg3 = determine_java_type(env, _arg3);
  int64_t arg4 = determine_java_type(env, _arg4);
  int64_t arg5 = determine_java_type(env, _arg5);
  int64_t arg6 = determine_java_type(env, _arg6);
  int64_t arg7 = determine_java_type(env, _arg7);
  int64_t arg8 = determine_java_type(env, _arg8);
  int64_t arg9 = determine_java_type(env, _arg9);
  STAP_PROBE10(HelperSDT, method__9, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9, rulename);
}

/*
 * Class:     HelperSDT
 * Method:    METHOD_STAP_PROBE10
 * Signature: (Ljava/lang/String;Ljava/lang/String;Ljava/lang/Object;Ljava/lang/Object;Ljava/lang/Object;Ljava/lang/Object;Ljava/lang/Object;Ljava/lang/Object;Ljava/lang/Object;Ljava/lang/Object;Ljava/lang/Object;Ljava/lang/Object;)V
 */
JNIEXPORT void JNICALL Java_org_systemtap_byteman_helper_HelperSDT_METHOD_1STAP_1PROBE10
(JNIEnv *env, jobject obj, jstring _rulename, jobject _arg1, jobject _arg2, jobject _arg3, jobject _arg4, jobject _arg5, jobject _arg6, jobject _arg7, jobject _arg8, jobject _arg9, jobject _arg10)
{
  char* rulename = get_java_string(env, _rulename);
  int64_t arg1 = determine_java_type(env, _arg1);
  int64_t arg2 = determine_java_type(env, _arg2);
  int64_t arg3 = determine_java_type(env, _arg3);
  int64_t arg4 = determine_java_type(env, _arg4);
  int64_t arg5 = determine_java_type(env, _arg5);
  int64_t arg6 = determine_java_type(env, _arg6);
  int64_t arg7 = determine_java_type(env, _arg7);
  int64_t arg8 = determine_java_type(env, _arg8);
  int64_t arg9 = determine_java_type(env, _arg9);
  int64_t arg10 = determine_java_type(env, _arg10);
  STAP_PROBE11(HelperSDT, method__10, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9, arg10, rulename);
}

/*
 * Class:     HelperSDT
 * Method:    METHOD_STAP_BT
 * Signature: (Ljava/lang/String;Ljava/lang/String;Ljava/lang/Integer;)V
 */
JNIEXPORT void JNICALL Java_org_systemtap_byteman_helper_HelperSDT_METHOD_1STAP_1BT
(JNIEnv *env, jobject obj, jstring _rulename, jstring _exception, jint _counter)
{
  char* rulename = get_java_string(env, _rulename);
  char* excp = get_java_string(env, _exception);
  int stdepth = _counter;
  STAP_PROBE3(HelperSDT, method__bt, excp, stdepth, rulename);
}

/*
 * Class:     HelperSDT
 * Method:    METHOD_BT_DELETE
 * Signature: (Ljava/lang/String;)V
 */
JNIEXPORT void JNICALL Java_org_systemtap_byteman_helper_HelperSDT_METHOD_1BT_1DELETE
(JNIEnv *env, jobject obj, jstring _rulename)
{
  char* rulename = get_java_string(env, _rulename);
  STAP_PROBE1(HelperSDT, method__bt__delete, rulename);
}

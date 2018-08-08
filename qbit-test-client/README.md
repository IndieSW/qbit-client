##QBit Test Client

qbit-test-client is a test client for the PushPoint Mobile event collector QBit test server and the C Language QBit Client. It attempts to send a defined number of QBit JSON ASCII Connection protocol messages to the server and expects to receive the same number of responses from the server.

##Building QBit Test Client for OS X

On OS X, Xcode may be used to build qbit-test-client by adding all of the source files to the project files list of an OS X Application/Command Line Tool project and:

    libwebsockets_shared.dylib
    libqbit.a

to the "Link Binary With Libraries" section of the project "Build Phases". For information about building libwebsockets and libqbit, see the README for the [C Language QBit Client](https://github.com/teamindra/qbit-client/tree/master/qbit-client-lib).

To simplify usage, the "Product Name" may be set to "qbit-test-client" in the "Packaging" section of the project "Build Settings".

##Manipulating the QBit Test Client App

###Server Host Name and Port

The server host name and port may be set in the function **main**, defined in qbit-test-client.c.

###Request Content

The method call message sent from the test client to the server is defined by the **QBIT_MESSAGE_BODY** character string in qbit-test-client.hp. The message string is a hardcoded format string which includes a placeholder for the timestamp (%ld). A value for the timestamp is provided when messages are formatted for transmission in the function **sendQbitMessages**.

###Response Content

Responses from the test server are tested to ensure they include the string "status: 'success'" as a very weak form of validation in the function **qbitMessageCallback**. The test string, **QBIT_SUCCESS_ARGUMENT**, is defined in qbit-test-client.hp.

###Number of Messages

**MAX_XR_COUNT**, defined qbit-test-client.hp, specifies the number of messages that will be sent by the test client to the server.

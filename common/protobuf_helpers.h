//======== Copyright © 2011, Valve Corporation, All rights reserved. ========//
//
// Purpose: Helper functions for protobufs
//
//===========================================================================//

namespace google
{
	namespace protobuf
	{
		class Message;
	}
}

// Given two protobuf objects, generate a third object containing only changed fields between the two objects
//bool ProtoBufDeltaMerge( const ::google::protobuf::Message &src, const ::google::protobuf::Message &delta, ::google::protobuf::Message* to );
//bool ProtoBufCalcDelta( const ::google::protobuf::Message &src, const ::google::protobuf::Message &dest, ::google::protobuf::Message *delta );


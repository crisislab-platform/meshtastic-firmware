# this script requires protoc and protoc-gen-nanopb to be installed and in your PATH

cd protobufs
protoc \
	--plugin=protoc-gen-nanopb=$(which protoc-gen-nanopb) \
	--experimental_allow_proto3_optional \
	"--nanopb_out=-S.cpp -v:../src/mesh/generated/" \
	-I=../protobufs meshtastic/*.proto

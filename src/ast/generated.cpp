#include <llvm/IR/DerivedTypes.h>

#include "ast.h"
#include "generated.h"
#include "logging.h"
#include "module_state.h"


Type* GeneratedType::getLLVMType(ModuleState& state) {
    if (type == KW_INT) {
        return Type::getInt32Ty(*state.ctx);
    } else if (type == KW_LONG) {
        return Type::getInt64Ty(*state.ctx);
    } else if (type == KW_FLOAT) {
        return Type::getFloatTy(*state.ctx);
    } else if (type == KW_DOUBLE) {
        return Type::getDoubleTy(*state.ctx);
    } else if (type == KW_BOOL) {
        return Type::getInt1Ty(*state.ctx);
    } else if (type == KW_VOID) {
        return Type::getVoidTy(*state.ctx);
    } else if (state.getStruct(type)) {
        // TODO: this will have to change a little to allow self and pre referencing
        return PointerType::getUnqual(*state.ctx);
    }

    return logError("type " + type + " not implemented yet");
}

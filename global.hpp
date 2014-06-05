#ifndef GLOBAL_HPP
#define GLOBAL_HPP

#include <QTypeInfo>

namespace Mesif {

enum CacheStates {
	StateModified,				// Modified - не совпадает с ОП и нет в других кэшах
	StateExclusive,				// Exclusive - совпадает с ОП и нет в других кэшах
	StateShared,				// Shared - совпадает с ОП и может быть в других кэшах
	StateInvalid,				// Invalid - недостоверна
	StateForward				// Forward - единственный ответчик для любых запросов
};

enum Messages {
	MessageRequestCell,			// Запрос ЯП
	MessageAbortRequestCell,	// Отмена запроса ЯП. Требуется повтор
	MessageValueCell,			// Значение ЯП
	MessageValueCellRam,		// Значение ЯП от ОП
	MessageSetInvalid,			// Установить ЯП в недостоверна
	MessageAbortRequestRam		// Отмена запроса ЯП в ОП
};

} //namespace Mesif

struct Action {
	quint32 adr;
	bool rw;
	int val;
};

struct Cell {
	quint32 adr;
	Mesif::CacheStates state;
	int val;
};

struct Message {
	int id;
	quint32 adr;
	Mesif::Messages type;
	int val;
};

#endif // GLOBAL_HPP

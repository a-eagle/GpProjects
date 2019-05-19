
var DB = {

_db : null,	
	
initDBDriver : function() {
	if (! this._db) {
		this._db = DBDriver.open('mysql://localhost:3306/tdx_f10', 'root', 'root');
	}
	return this._db;
},

metaToArray : function(meta) {
	var title = [];
	var colNum = meta.getColumnCount();
	for (var i = 0; i < colNum; ++i) {
		title.push(meta.getColumnLabel(i));
	}
	return title;
},

/**
*	@param rs ResultSet object
*   @param columnNames An array of string, result set column names
*   @return An array of [{}, {}, ...]
*/
resultToArray : function (rs, columnNames) {
	var meta = rs.getMetaData();
	var colNum = meta.getColumnCount();
	var data = [];
	var title = [];
	if (columnNames && Array.isArray(columnNames) && colNum <= columnNames.length) {
		title = columnNames;
	} else {
		title = this.metaToArray(meta);
	}
	
	while (rs.next()) {
		var robj = {};
		for (var i = 0; i < colNum; ++i) {
			robj[title[i]] = rs.getString(i);
			// alert(title[i] + "  -> " + robj[title[i]]);
		}
		data.push(robj);
	}
	return data;
},

// should close ResultSet yourself
query : function(sql, rsToArray) {
	var db = this.initDBDriver();
	var rs = db.executeQuery(sql);
	if (! rsToArray) {
		return rs;
	}
	var data = this.resultToArray(rs);
	rs.close();
	return data;
},

/**
*	@bindParams An arroy of string, sql bind params
*	@return return An array of record object
*      should close ResultSet yourself
*/
prepareQuery : function (sql, bindParams, rsToArray) {
	var db = this.initDBDriver();
	var ps = db.prepare(sql);
	var paramNum = ps.getParameterCount();
	if (paramNum > 0 && paramNum != bindParams.length) {
		alert('prepareQuery: bind params number error');
		return null;
	}
	for (var i = 0; i < paramNum; ++i) {
		ps.setString(i, bindParams[i]);
	}
	var rs = ps.executeQuery();
	if (! rsToArray) {
		return rs;
	}
	var data = this.resultToArray(rs);
	rs.close();
	// ps.close();
	
	return data;
}

};








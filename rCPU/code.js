$(function() {
  $("#status").text("OK. Javascript and jQuery are working...");
  
  setInterval("count_with_ajax()", 1000);
});

function count_with_ajax()
{
	$.ajax({
		url: "counter.api",
		type: "post",
		data: { counter:$("#counter").text() }
	}).done(function(data)
	{
		$("#counter").text(data);
	});
}
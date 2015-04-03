$(function() {
  $("#status").text("");
  setInterval("get_cpu_use()", 500);
});

function get_cpu_use()
{
	$.ajax({
		url: "cpu.api",
		type: "post",
		data: { counter:$("#counter").text() }
	}).done(function(data)
	{
		$("#counter").text(data[1]);
	});
}